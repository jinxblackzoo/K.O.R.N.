/*
 * K.O.R.N. - Katastrophal Organisierter Nahrungsmittel Spender
 * Arduino UNO R4 WiFi - Code mit DS3231 Echtzeituhr für eine präzise tägliche Fütterungszeit
 * 
 * VERDRAHTUNG DS3231:
 * DS3231 VCC  → Arduino 3.3V (oder 5V)
 * DS3231 GND  → Arduino GND
 * DS3231 SDA  → Arduino A4 (SDA)
 * DS3231 SCL  → Arduino A5 (SCL)
 * DS3231 SQW  → NICHT anschließen! 
 * 
 * VERDRAHTUNG MOTORTREIBER:
 * Treiber PUL+ → Arduino Pin 2 (Schrittimpulse)
 * Treiber DIR+ → Arduino Pin 3 (Drehrichtung)
 * Treiber ENA+ → Arduino Pin 5 (Motor aktivieren)
 * Relais IN   → Arduino Pin 8 (Stromversorgung Treiber)
 * 
 * SERIELLER MONITOR: 9600 Baud
 */

#include <AccelStepper.h>
#include <RTClib.h>
#include <Wire.h>
#include <EEPROM.h>   // EEPROM für Arduino UNO R4 WiFi (flash-emuliert, 256 Bytes)

// ============================================================================
// KONFIGURATION - HIER ALLE PARAMETER EINSTELLEN
// ============================================================================

// FÜTTERUNGSZEIT (24h Format)
const int FUETTERUNG_STUNDE_1 = 7;    // Fütterung: 07:01 Uhr. ACHTUNG: Keine führende Null (z.B. 09) - C++ wertet das als Oktalzahl!
const int FUETTERUNG_MINUTE_1 = 1;

// MOTORPARAMETER
const int MOTOR_BESCHLEUNIGUNG = 1000; // Beschleunigung in Steps/s²
// Zeitbasierte Dosierung: feste Schrittfrequenz + Dauer in Sekunden
const int SCHRITTFREQUENZ = 1000;      // Steps pro Sekunde (fix)
const int FUETTERUNG_DAUER_SEK = 3;    // Fütterungsdauer in Sekunden (1–60 s)
const bool MOTOR_RECHTS = false;        // true = Rechts (CW), false = Links (CCW)

// TIMING-PARAMETER
const int RELAIS_VERZOEGERUNG = 2000;  // Verzögerung zwischen Relais-Ein und Motor-Start (ms)
const int MOTOR_PAUSE = 5000;          // Pause nach Motorlauf bevor Relais ausschaltet (ms)
const int LOOP_INTERVALL = 10;         // Pause zwischen zwei Loop-Durchläufen in Sekunden
const int ZEIT_AUSGABE_INTERVALL = 10; // Statusausgabe alle 10 Sekunden
const int FUETTERUNGS_FENSTER_MIN = 2; // Bis Zielzeit +2 Min: Logtext 'Fütterung', danach 'Nachholen' (nur Anzeige, keine Logik)
const int NACHHOLFENSTER_MIN = 60;    // Nachholfenster: Zielzeit bis +60 Minuten

// AUTOMATISCHE ZEITSYNCHRONISATION
const bool ZEIT_EINSTELLEN = false;    // Nur setzen, wenn lostPower() oder explizit gewünscht

// ============================================================================
// PIN-DEFINITIONEN
// ============================================================================
#define PUL_PIN 2    // Schrittimpulse zum Motortreiber
#define DIR_PIN 3    // Drehrichtung zum Motortreiber  
#define OPTO_PIN 4   // Optokoppler (Reserve)
#define ENA_PIN 5    // Motor aktivieren/deaktivieren
#define RELAY_PIN 8  // Relais für Motortreiber-Stromversorgung

// JUMPER-RESET: Pin 12 und 13 verbinden -> EEPROM-Fütterungsdatum löschen
#define JUMPER_PIN_A 12  // Eingang mit internem Pullup
#define JUMPER_PIN_B 13  // GND-Seite (auf OUTPUT LOW)

// ============================================================================
// OBJEKTE UND VARIABLEN
// ============================================================================
AccelStepper stepper(AccelStepper::DRIVER, PUL_PIN, DIR_PIN);
RTC_DS3231 rtc;

// Zustandsvariable für serielle Statusausgabe
unsigned long letzte_ausgabe_ms = 0;  // millis() der letzten seriellen Zeitausgabe (nicht-blockierend)
unsigned long jumperGedruecktMs = 0;  // millis() zu dem Jumper erstmals erkannt wurde

// ============================================================================
// EEPROM - LETZTE FÜTTERUNGSZEIT
// ============================================================================
// Der UNO R4 WiFi nutzt ein flash-emuliertes EEPROM (Data Flash).
// Hier speichern wir das Datum der letzten Fütterung persistent.
const int EEPROM_ADRESSE = 0;

struct LetzteFuetterung {
  uint16_t jahr;
  uint8_t monat;
  uint8_t tag;
  uint8_t stunde;
  uint8_t minute;
};

// Vorwärtsdeklarationen der EEPROM-/Statusfunktionen
void letzte_fuetterung_speichern(DateTime dt);
bool letzte_fuetterung_laden(DateTime &dt, DateTime jetzt);
bool fuetterung_ab_datum(DateTime zielDatum, DateTime letzte);
void zeige_datum(DateTime dt);
void zeige_uhrzeit(DateTime dt);
void zeige_fuetterungsstatus(DateTime jetzt);

// ============================================================================
// JUMPER-RESET: Pin 12 und 13 verbinden -> EEPROM-Fütterungsdatum löschen
// ============================================================================
// sofort=true: löscht sofort beim Start (nur in setup)
// sofort=false: löscht erst nach 3 Sekunden Dauer-Kontakt (im laufenden Betrieb)
// Pin-Konfiguration erfolgt einmalig in setup()
void checkJumperReset(bool sofort) {
  if (digitalRead(JUMPER_PIN_A) == LOW) {
    if (jumperGedruecktMs == 0) jumperGedruecktMs = millis();
    if (sofort || (millis() - jumperGedruecktMs >= 3000UL)) {
      Serial.println("🔌 Jumper Pin 12↔13 erkannt - Fütterungsdatum wird gelöscht...");
      for (int i = 0; i < (int)sizeof(LetzteFuetterung); i++) {
        EEPROM.write(EEPROM_ADRESSE + i, 0xFF);
      }
      Serial.println("✅ EEPROM gelöscht. Jumper entfernen, um fortzufahren...");
      while (digitalRead(JUMPER_PIN_A) == LOW) {
        delay(100);
      }
      Serial.println("🔌 Jumper entfernt - normaler Start.");
      jumperGedruecktMs = 0;
    }
  } else {
    jumperGedruecktMs = 0;
  }
}

void setup() {
  // Pins früh konfigurieren, bevor Ausgänge erstmals genutzt werden
  pinMode(ENA_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  // Anfangszustand der Ausgänge
  digitalWrite(ENA_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);

  Serial.begin(9600);              // Serieller Monitor: 9600 Baud
  Serial.println("===========================================");
  Serial.println("🐓 K.O.R.N. Fütterungsautomat gestartet!");
  Serial.println("===========================================");

  // Jumper-Pins einmalig konfigurieren (Pin 12 = Eingang, Pin 13 = GND-Seite)
  pinMode(JUMPER_PIN_B, OUTPUT);
  digitalWrite(JUMPER_PIN_B, LOW);
  pinMode(JUMPER_PIN_A, INPUT_PULLUP);
  delay(10);   // Pullup einschwingen lassen, sonst droht ein Fehl-LOW beim ersten Lesen

  // Jumper-Check: Pin 12 ↔ 13 verbunden = EEPROM-Fütterungsdatum löschen
  checkJumperReset(true);
  
  // I2C starten und RTC initialisieren
  Wire.begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true); // 3s Timeout, bei Fehlern Wire zurücksetzen
#endif
  if (!rtc.begin()) {
    Serial.println("FEHLER: DS3231 nicht gefunden!");
    Serial.println("Prüfe Verdrahtung:");
    Serial.println("VCC → 3.3V, GND → GND, SDA → A4, SCL → A5");
    // Dauerhafter Fehler-Alarm: alle 5 Sekunden neuer Verbindungsversuch zur RTC
    while (!rtc.begin()) {
      safeDelay(5000);
    }
    Serial.println("DS3231 wieder gefunden - fahre fort.");
  }
  
  // Zeit und Datum einstellen (automatisch bei Bedarf)
  bool rtc_verlust = false;
  if (rtc.lostPower()) {
    rtc_verlust = true;
    Serial.println("⚠️ RTC verlor Versorgung – Zeit wird neu gesetzt.");
  }
  if (rtc_verlust || ZEIT_EINSTELLEN) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("📡 DS3231 RTC initialisiert / Zeit gesetzt");
  } else {
    Serial.println("⏱️ RTC-Zeit bleibt unverändert (kein Verlust erkannt)");
  }
  // Sanity-Check: Falls RTC-Zeit offensichtlich hinter der Build-Zeit liegt, angleichen
  {
    DateTime build(F(__DATE__), F(__TIME__));
    DateTime nowCheck = rtc.now();
    if (nowCheck.unixtime() + 60 < build.unixtime()) { // >60 s hinter Build-Zeit
      Serial.println("⚠️ RTC-Zeit liegt vor Build-Zeit – setze auf Build-Zeit.");
      rtc.adjust(build);
    }
  }
  
  // Synchronisierte/aktuelle Zeit einmalig anzeigen
  {
    DateTime jetzt = rtc.now();
    Serial.print("📅 Datum: ");
    Serial.print(jetzt.day(), DEC);
    Serial.print(".");
    Serial.print(jetzt.month(), DEC);
    Serial.print(".");
    Serial.print(jetzt.year(), DEC);
    Serial.println();
  }
  
  // Aktuelle Zeit anzeigen
  zeige_aktuelle_zeit(rtc.now());
  
  // Motor konfigurieren (zeitbasierte Dosierung): feste Schrittfrequenz
  stepper.setMaxSpeed(SCHRITTFREQUENZ);
  stepper.setAcceleration(MOTOR_BESCHLEUNIGUNG);
  
  // Pins wurden bereits zu Beginn von setup() konfiguriert und initialisiert
  
  Serial.println("-------------------------------------------");
  Serial.println("Konfigurierte Fütterungszeit:");
  Serial.print("Fütterung: "); 
  formatiere_zeit(FUETTERUNG_STUNDE_1, FUETTERUNG_MINUTE_1);
  Serial.println("-------------------------------------------");
  zeige_fuetterungsstatus(rtc.now());
  Serial.println("System bereit! Loop alle 10 s, Statusausgabe alle 10 s.");
  Serial.println("===========================================");
}

void loop() {
  // Jumper-Reset auch im laufenden Betrieb ermöglichen (mind. 3 s halten)
  checkJumperReset(false);

  DateTime jetzt = rtc.now();

  // Plausibilitätsprüfung: bei I2C-Störung können Müllwerte kommen
  if (jetzt.year() < 2024 || jetzt.year() > 2099 ||
      jetzt.month() < 1 || jetzt.month() > 12 ||
      jetzt.day() < 1 || jetzt.day() > 31 ||
      jetzt.hour() > 23 || jetzt.minute() > 59 || jetzt.second() > 59) {
    Serial.println("⚠️ Ungültige RTC-Zeit gelesen - Durchlauf wird übersprungen!");
    safeDelay(1000);
    return;
  }

  // Statusausgabe alle ZEIT_AUSGABE_INTERVALL Sekunden (nicht-blockierend über millis())
  unsigned long nowMs = millis();
  if (nowMs - letzte_ausgabe_ms >= (unsigned long)ZEIT_AUSGABE_INTERVALL * 1000UL) {
    zeige_aktuelle_zeit(jetzt);
    zeige_fuetterungsstatus(jetzt);
    letzte_ausgabe_ms = nowMs;
  }

  // Letztes Fütterungsdatum aus EEPROM laden
  DateTime letzte;
  bool letzteGueltig = letzte_fuetterung_laden(letzte, jetzt);

  // Fütterungsfenster berechnen (Zielzeit heute und gestern, da das 60-Min-Fenster über Mitternacht greifen kann)
  DateTime zielHeute = DateTime(jetzt.year(), jetzt.month(), jetzt.day(), FUETTERUNG_STUNDE_1, FUETTERUNG_MINUTE_1, 0);
  DateTime zielGestern = zielHeute - TimeSpan(86400L);
  DateTime heuteFensterEnde = zielHeute + TimeSpan((long)NACHHOLFENSTER_MIN * 60L);
  DateTime gesternFensterEnde = zielGestern + TimeSpan((long)NACHHOLFENSTER_MIN * 60L);

  bool imHeutigenFenster = (jetzt >= zielHeute && jetzt <= heuteFensterEnde);
  bool imGestrigenFenster = (jetzt >= zielGestern && jetzt <= gesternFensterEnde);

  // Prüfen, ob für das jeweilige Fenster noch nicht gefüttert wurde
  bool heuteFutterNoetig = imHeutigenFenster && (!letzteGueltig || !fuetterung_ab_datum(zielHeute, letzte));
  bool gesternFutterNoetig = imGestrigenFenster && (!letzteGueltig || !fuetterung_ab_datum(zielGestern, letzte));

  if (heuteFutterNoetig || gesternFutterNoetig) {
    DateTime zielAktuell = heuteFutterNoetig ? zielHeute : zielGestern;
    // Logtext-Unterscheidung anhand der tatsächlichen Erkennungszeit
    TimeSpan seitZiel = jetzt - zielAktuell;

    if (seitZiel.totalseconds() <= (long)FUETTERUNGS_FENSTER_MIN * 60L) {
      Serial.println("🍽️ FÜTTERUNG STARTET...");
    } else {
      Serial.println("🍽️ Fütterung startet (Nachholen)...");
    }

    // Datum sofort speichern, BEVOR der Motor anfährt, um Doppelfütterungen zu vermeiden.
    // Beim Nachholen des gestrigen Fensters (Mitternacht-Übergang) wird der gestrige
    // Zielzeitpunkt gespeichert, damit die heutige Fütterung nicht als erledigt gilt.
    DateTime gespeichert = heuteFutterNoetig ? jetzt : zielGestern;
    letzte_fuetterung_speichern(gespeichert);
    Serial.print("💾 Fütterungsdatum gespeichert: ");
    zeige_datum(gespeichert);
    Serial.print(" ");
    zeige_uhrzeit(gespeichert);
    if (!heuteFutterNoetig) {
      Serial.print(" (nachgeholt am ");
      zeige_datum(jetzt);
      Serial.print(" um ");
      zeige_uhrzeit(jetzt);
      Serial.print(")");
    }
    Serial.println();

    fuetterungsvorgang();
    Serial.println("✅ Fütterung abgeschlossen!");
  }

  // Pause zwischen zwei Loop-Durchläufen, dabei regelmäßig Jumper prüfen
  unsigned long loopStart = millis();
  while (millis() - loopStart < (unsigned long)LOOP_INTERVALL * 1000UL) {
    checkJumperReset(false);
    delay(50);
  }
}

// ============================================================================
// FÜTTERUNGSVORGANG - Motor ansteuern
// ============================================================================
void fuetterungsvorgang() {
  Serial.println("→ Relais aktivieren...");
  digitalWrite(RELAY_PIN, HIGH);           // Motortreiber mit Strom versorgen
  safeDelay(RELAIS_VERZOEGERUNG);          // Warten bis Treiber bereit
  
  Serial.println("→ Motor aktivieren...");
  digitalWrite(ENA_PIN, HIGH);             // Motor aktivieren
  
  // Schritte für diese Fütterung aus Dauer berechnen (1–60 s)
  const int sek = constrain(FUETTERUNG_DAUER_SEK, 1, 60);
  const long schritte = (long)sek * SCHRITTFREQUENZ;
  Serial.print("→ Motor läuft ");
  Serial.print(sek);
  Serial.print(" s = ");
  Serial.print(schritte);
  Serial.println(" Schritte...");
  
  if (MOTOR_RECHTS) {
    stepper.move(schritte);                // relative Bewegung im Uhrzeigersinn
  } else {
    stepper.move(-schritte);               // relative Bewegung gegen Uhrzeigersinn
  }
  // Bewegung ausführen (blockierend)
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  
  Serial.println("→ Motor stoppen...");
  digitalWrite(ENA_PIN, LOW);              // Motor deaktivieren
  
  safeDelay(MOTOR_PAUSE);                  // Kurze Pause
  
  Serial.println("→ Relais deaktivieren...");
  digitalWrite(RELAY_PIN, LOW);            // Motortreiber stromlos schalten
}

// ============================================================================
// HILFSFUNKTIONEN FÜR ZEITAUSGABE
// ============================================================================
void zeige_aktuelle_zeit(DateTime jetzt) {
  Serial.print(" Aktuelle Zeit: ");
  
  // Datum formatieren
  if (jetzt.day() < 10) Serial.print("0");
  Serial.print(jetzt.day());
  Serial.print(".");
  if (jetzt.month() < 10) Serial.print("0");
  Serial.print(jetzt.month());
  Serial.print(".");
  Serial.print(jetzt.year());
  Serial.print(" ");
  
  // Uhrzeit formatieren
  if (jetzt.hour() < 10) Serial.print("0");
  Serial.print(jetzt.hour());
  Serial.print(":");
  if (jetzt.minute() < 10) Serial.print("0");
  Serial.print(jetzt.minute());
  Serial.print(":");
  if (jetzt.second() < 10) Serial.print("0");
  Serial.print(jetzt.second());
  
  // Temperatur vom DS3231 anzeigen (Bonus-Feature)
  Serial.print(" | Temp: ");
  Serial.print(rtc.getTemperature());
  Serial.println("°C");
}

// ============================================================================
// EEPROM: LETZTE FÜTTERUNGSZEIT SPEICHERN/LADEN
// ============================================================================
void letzte_fuetterung_speichern(DateTime dt) {
  LetzteFuetterung lf;
  lf.jahr = dt.year();
  lf.monat = dt.month();
  lf.tag = dt.day();
  lf.stunde = dt.hour();
  lf.minute = dt.minute();
  EEPROM.put(EEPROM_ADRESSE, lf);
}

bool letzte_fuetterung_laden(DateTime &dt, DateTime jetzt) {
  LetzteFuetterung lf;
  EEPROM.get(EEPROM_ADRESSE, lf);

  // Gültigkeitsprüfung (leerer/uninitialisierter EEPROM hat 0xFF = 65535/255)
  if (lf.jahr < 2024 || lf.jahr > 2099) return false;
  if (lf.monat < 1 || lf.monat > 12) return false;
  if (lf.tag < 1 || lf.tag > 31) return false;

  // Abwärtskompatibilität: alter 4-Byte-Eintrag ohne Uhrzeit -> 00:00 annehmen
  if (lf.stunde == 0xFF && lf.minute == 0xFF) {
    lf.stunde = 0;
    lf.minute = 0;
  }

  if (lf.stunde > 23 || lf.minute > 59) return false;

  dt = DateTime(lf.jahr, lf.monat, lf.tag, lf.stunde, lf.minute, 0);

  // Zukunftsdatum ist ungültig (z. B. nach RTC-Reset auf ältere Build-Zeit).
  // Sonst würde bis zum gespeicherten Datum gar nicht mehr gefüttert.
  DateTime heute0(jetzt.year(), jetzt.month(), jetzt.day(), 0, 0, 0);
  if (dt >= heute0 + TimeSpan(86400L)) return false;

  return true;
}

void zeige_datum(DateTime dt) {
  if (dt.day() < 10) Serial.print("0");
  Serial.print(dt.day());
  Serial.print(".");
  if (dt.month() < 10) Serial.print("0");
  Serial.print(dt.month());
  Serial.print(".");
  Serial.print(dt.year());
}

void zeige_uhrzeit(DateTime dt) {
  if (dt.hour() < 10) Serial.print("0");
  Serial.print(dt.hour());
  Serial.print(":");
  if (dt.minute() < 10) Serial.print("0");
  Serial.print(dt.minute());
}

bool fuetterung_ab_datum(DateTime zielDatum, DateTime letzte) {
  // true, wenn die letzte Fütterung am zielDatum oder danach stattfand
  return (letzte.year() > zielDatum.year()) ||
         (letzte.year() == zielDatum.year() && letzte.month() > zielDatum.month()) ||
         (letzte.year() == zielDatum.year() && letzte.month() == zielDatum.month() && letzte.day() >= zielDatum.day());
}

// ============================================================================
// FÜTTERUNGSSTATUS: LETZTE FÜTTERUNG UND ZEIT BIS ZUR NÄCHSTEN
// ============================================================================
void zeige_fuetterungsstatus(DateTime jetzt) {
  Serial.print("🍽️ Letzte Fütterung: ");
  DateTime letzte;
  bool letzteGueltig = letzte_fuetterung_laden(letzte, jetzt);
  if (letzteGueltig) {
    zeige_datum(letzte);
    Serial.print(" ");
    zeige_uhrzeit(letzte);
  } else {
    Serial.print("noch keine");
  }

  Serial.print(" | ⏳ Nächste: ");

  // Zielzeit heute, gestern (Mitternacht-Übergang) und morgen
  DateTime zielHeute = DateTime(jetzt.year(), jetzt.month(), jetzt.day(), FUETTERUNG_STUNDE_1, FUETTERUNG_MINUTE_1, 0);
  DateTime zielGestern = zielHeute - TimeSpan(86400L);
  DateTime zielMorgen = zielHeute + TimeSpan(86400L);
  DateTime heuteFensterEnde = zielHeute + TimeSpan((long)NACHHOLFENSTER_MIN * 60L);
  DateTime gesternFensterEnde = zielGestern + TimeSpan((long)NACHHOLFENSTER_MIN * 60L);

  bool imHeutigenFenster = (jetzt >= zielHeute && jetzt <= heuteFensterEnde);
  bool imGestrigenFenster = (jetzt >= zielGestern && jetzt <= gesternFensterEnde);

  // Heute schon gefüttert? (auch wenn im gestrigen Fenster aufgeholt wurde)
  DateTime heuteDatum(jetzt.year(), jetzt.month(), jetzt.day(), 0, 0, 0);
  bool heuteGefuettert = letzteGueltig && fuetterung_ab_datum(heuteDatum, letzte);

  DateTime naechsteFuetterung;
  if (imHeutigenFenster && (!letzteGueltig || !fuetterung_ab_datum(zielHeute, letzte))) {
    naechsteFuetterung = jetzt;  // innerhalb des heutigen Fensters fällig
  } else if (imGestrigenFenster && (!letzteGueltig || !fuetterung_ab_datum(zielGestern, letzte))) {
    naechsteFuetterung = jetzt;  // innerhalb des gestrigen Nachholfensters fällig
  } else if (heuteGefuettert || jetzt > heuteFensterEnde) {
    naechsteFuetterung = zielMorgen;  // nächste Fütterung morgen
  } else {
    naechsteFuetterung = zielHeute;   // nächste Fütterung heute zum Zielzeitpunkt
  }

  zeige_datum(naechsteFuetterung);
  Serial.print(" ");
  zeige_uhrzeit(naechsteFuetterung);
  Serial.print(" (in ");

  TimeSpan diff = naechsteFuetterung - jetzt;
  long verbleibend = diff.totalseconds();
  if (verbleibend < 0) verbleibend = 0;

  Serial.print(verbleibend / 3600);
  Serial.print("h ");
  Serial.print((verbleibend % 3600) / 60);
  Serial.print("m ");
  Serial.print(verbleibend % 60);
  Serial.println("s)");
}

// ============================================================================
// ZEITHELFER: blockierende Verzögerung in kleinen Schritten
// ============================================================================
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    delay(50);
  }
}

void formatiere_zeit(int stunde, int minute) {
  if (stunde < 10) Serial.print("0");
  Serial.print(stunde);
  Serial.print(":");
  if (minute < 10) Serial.print("0");
  Serial.print(minute);
  Serial.println(" Uhr");
}
