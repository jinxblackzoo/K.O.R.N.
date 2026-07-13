/*
 * K.O.R.N. - Katastrophal Organisierter Nahrungsmittel Spender
 * Arduino-Code mit DS3231 Echtzeituhr für eine präzise tägliche Fütterungszeit
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
 * VERDRAHTUNG BUZZER (AKTIV):
 * Buzzer VCC  → Arduino 5V (oder VIN für lauter)
 * Buzzer GND  → Arduino GND
 * Buzzer SIG  → Arduino Pin 6 (HIGH = Ton, LOW = Stille)
 * 
 * HINWEIS: Aktiver Buzzer hat eigenen Oszillator eingebaut.
 * HIGH-Signal aktiviert den Ton, LOW-Signal schaltet ihn aus.
 * 
 * HINWEIS: VIN liefert die rohe Eingangsspannung (7-12V je nach Netzteil).
 * Der Buzzer wird dadurch lauter, aber DS3231 MUSS an 3.3V/5V bleiben!
 */

#include <AccelStepper.h>
#include <RTClib.h>
#include <Wire.h>
#ifdef __AVR__
#include <avr/wdt.h>
#endif

// ============================================================================
// KONFIGURATION - HIER ALLE PARAMETER EINSTELLEN
// ============================================================================

// FÜTTERUNGSZEIT (24h Format)
const int FUETTERUNG_STUNDE_1 = 7;    // Fütterung. ACHTUNG: Keine führende Null (z.B. 09) - C++ wertet das als Oktalzahl!
const int FUETTERUNG_MINUTE_1 = 1;

// MOTORPARAMETER
const int MOTOR_BESCHLEUNIGUNG = 2000; // Beschleunigung in Steps/s²
// Zeitbasierte Dosierung wie in Rev3: feste Schrittfrequenz + Dauer in Sekunden
const int SCHRITTFREQUENZ = 1000;      // Steps pro Sekunde (fix)
const int FUETTERUNG_DAUER_SEK = 5;    // Fütterungsdauer in Sekunden (1–60 s)
const bool MOTOR_RECHTS = false;        // true = Rechts (CW), false = Links (CCW)

// TIMING-PARAMETER
const int RELAIS_VERZOEGERUNG = 2000;  // Verzögerung zwischen Relais-Ein und Motor-Start (ms)
const int MOTOR_PAUSE = 5000;          // Pause nach Motorlauf bevor Relais ausschaltet (ms)
const int ZEIT_AUSGABE_INTERVALL = 30; // Zeitausgabe alle 30 Sekunden

// SOUND-PARAMETER
const int BUZZER_VORWARNUNG = 3;       // Vorwarnung 3 Sekunden vor Fütterung
const int BUZZER_ANZAHL_TOENE = 3;     // Anzahl der Warntöne
const int BUZZER_TON_DAUER = 1000;     // Dauer eines Tons in ms
const int BUZZER_PAUSE_DAUER = 200;    // Pause zwischen Tönen in ms

// AUTOMATISCHE ZEITSYNCHRONISATION
const bool ZEIT_EINSTELLEN = false;    // Nur setzen, wenn lostPower() oder explizit gewünscht

// ============================================================================
// PIN-DEFINITIONEN
// ============================================================================
#define PUL_PIN 2    // Schrittimpulse zum Motortreiber
#define DIR_PIN 3    // Drehrichtung zum Motortreiber  
#define OPTO_PIN 4   // Optokoppler (Reserve)
#define ENA_PIN 5    // Motor aktivieren/deaktivieren
#define BUZZER_PIN 6 // Aktiver Buzzer für Fütterungswarnung (HIGH/LOW)
#define RELAY_PIN 8  // Relais für Motortreiber-Stromversorgung

// ============================================================================
// OBJEKTE UND VARIABLEN
// ============================================================================
AccelStepper stepper(AccelStepper::DRIVER, PUL_PIN, DIR_PIN);
RTC_DS3231 rtc;

// Zustandsvariablen für Fütterungslogik
bool fuetterung1_heute = false;  // Fütterung heute bereits erfolgt?
int letzter_tag = -1;            // Letzter Tag für Tagesreset

void setup() {
  // Pins früh konfigurieren, bevor Ausgänge erstmals genutzt werden
  pinMode(ENA_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  // Anfangszustand der Ausgänge
  digitalWrite(ENA_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  // ===================
  // POWERUP-SIGNALTON: 2x 1 Sekunde mit 1 Sekunde Pause
  // ===================
  // Signalisiert, dass der Arduino frisch mit Strom versorgt wurde (Powerup)
  // 1. Buzzer 1 Sekunde AN
  digitalWrite(BUZZER_PIN, HIGH);   // Buzzer EIN
  safeDelay(1000);                  // 1 Sekunde warten
  digitalWrite(BUZZER_PIN, LOW);    // Buzzer AUS
  safeDelay(1000);                  // 1 Sekunde Pause
  // 2. Buzzer 1 Sekunde AN
  digitalWrite(BUZZER_PIN, HIGH);   // Buzzer EIN
  safeDelay(1000);                  // 1 Sekunde warten
  digitalWrite(BUZZER_PIN, LOW);    // Buzzer AUS
  // ===================


  Serial.begin(9600);
  Serial.println("===========================================");
  Serial.println("🐓 K.O.R.N. Fütterungsautomat gestartet!");
  Serial.println("===========================================");
  
  // I2C starten und RTC initialisieren
  Wire.begin();
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true); // 3s Timeout, bei Fehlern Wire zurücksetzen
#endif
  if (!rtc.begin()) {
    Serial.println("FEHLER: DS3231 nicht gefunden!");
    Serial.println("Prüfe Verdrahtung:");
    Serial.println("VCC → 3.3V, GND → GND, SDA → A4, SCL → A5");
    // Dauerhafter Fehler-Alarm: 1x kurzer Beep alle 5 Sekunden,
    // dazwischen erneuter Verbindungsversuch zur RTC
    while (!rtc.begin()) {
      digitalWrite(BUZZER_PIN, HIGH);
      safeDelay(200);
      digitalWrite(BUZZER_PIN, LOW);
      safeDelay(4800);
    }
    Serial.println("DS3231 wieder gefunden - fahre fort.");
  }
  
#ifdef __AVR__
  // Watchdog auf 2s aktivieren
  wdt_enable(WDTO_2S);
#endif
  
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
  zeige_aktuelle_zeit();
  
  // Motor konfigurieren (zeitbasierte Dosierung): feste Schrittfrequenz
  stepper.setMaxSpeed(SCHRITTFREQUENZ);
  stepper.setAcceleration(MOTOR_BESCHLEUNIGUNG);
  
  // Pins wurden bereits zu Beginn von setup() konfiguriert und initialisiert
  
  Serial.println("🎺 Upload erfolgreich - Fanfare wird abgespielt...");
  fanfare_abspielen();
  
  Serial.println("-------------------------------------------");
  Serial.println("Konfigurierte Fütterungszeit:");
  Serial.print("Fütterung: "); 
  formatiere_zeit(FUETTERUNG_STUNDE_1, FUETTERUNG_MINUTE_1);
  Serial.println("-------------------------------------------");
  Serial.println("System bereit! Zeitausgabe alle 30 Sekunden.");
  Serial.println("===========================================");
}

void loop() {
  // Watchdog regelmäßig füttern
#ifdef __AVR__
  wdt_reset();
#endif
  DateTime jetzt = rtc.now();
  
  // Plausibilitätsprüfung: bei I2C-Störung können Müllwerte kommen
  if (jetzt.year() < 2024 || jetzt.year() > 2099) {
    Serial.println("⚠️ Ungültige RTC-Zeit gelesen - Durchlauf wird übersprungen!");
    safeDelay(5000);
    return;
  }
  
  // Zeitausgabe bei jedem Loop-Durchlauf (Loop läuft alle ZEIT_AUSGABE_INTERVALL Sekunden)
  zeige_aktuelle_zeit();
  
  // Tagesreset um Mitternacht - Fütterungsflags zurücksetzen
  if (jetzt.day() != letzter_tag) {
    fuetterung1_heute = false;
    letzter_tag = jetzt.day();
    
    // Neues Datum anzeigen
    Serial.print("📅 NEUER TAG: ");
    Serial.print(jetzt.day(), DEC);
    Serial.print(".");
    Serial.print(jetzt.month(), DEC);
    Serial.print(".");
    Serial.print(jetzt.year(), DEC);
    Serial.print(" ");
    if (jetzt.hour() < 10) Serial.print("0");
    Serial.print(jetzt.hour());
    Serial.print(":");
    if (jetzt.minute() < 10) Serial.print("0");
    Serial.print(jetzt.minute());
    Serial.println(" - Flags zurückgesetzt (Heartbeat)");
  }
  
  // Fütterungszeit prüfen - FÜTTERUNG
  if (!fuetterung1_heute && 
      jetzt.hour() == FUETTERUNG_STUNDE_1 && 
      jetzt.minute() == FUETTERUNG_MINUTE_1) {
    
    Serial.println("🍽️ FÜTTERUNG STARTET...");
    fuetterungsvorgang();
    fuetterung1_heute = true;
    Serial.println("✅ Fütterung abgeschlossen!");
  }
  
  // Warten bevor nächste Zeitprüfung
  // Verhindert mehrfache Fütterung in derselben Minute
  safeDelay((unsigned long)ZEIT_AUSGABE_INTERVALL * 1000UL);
}

// ============================================================================
// FÜTTERUNGSVORGANG - Motor ansteuern
// ============================================================================
void fuetterungsvorgang() {
  // 🔊 BUZZER-WARNUNG: 3x Beep für je 1 Sekunde
  Serial.println("🔊 Fütterungswarnung: 3x Beep...");
  buzzer_warnung();
  
  // Vorwarnzeit nach Buzzer-Warnung abwarten
  Serial.print("⏱️ ");
  Serial.print(BUZZER_VORWARNUNG);
  Serial.println(" Sekunden warten...");
  safeDelay((unsigned long)BUZZER_VORWARNUNG * 1000UL);
  
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
  // Bewegung ausführen (blockierend), dabei Watchdog füttern
  while (stepper.distanceToGo() != 0) {
    stepper.run();
#ifdef __AVR__
    wdt_reset();
#endif
  }
  
  Serial.println("→ Motor stoppen...");
  digitalWrite(ENA_PIN, LOW);              // Motor deaktivieren
  
  safeDelay(MOTOR_PAUSE);                  // Kurze Pause
  
  Serial.println("→ Relais deaktivieren...");
  digitalWrite(RELAY_PIN, LOW);            // Motortreiber stromlos schalten
}

// ============================================================================
// BUZZER-WARNUNG (AKTIVER BUZZER)
// ============================================================================
void buzzer_warnung() {
  Serial.println("→ Buzzer-Warnung: 3 Töne à 1 Sekunde");
  
  for (int i = 0; i < BUZZER_ANZAHL_TOENE; i++) {
    Serial.print("  Ton "); Serial.print(i + 1); Serial.println("...");
    
    // Aktiven Buzzer mit HIGH-Signal aktivieren
    digitalWrite(BUZZER_PIN, HIGH);      // Buzzer EIN
    safeDelay(BUZZER_TON_DAUER);         // 1 Sekunde Ton
    digitalWrite(BUZZER_PIN, LOW);       // Buzzer AUS
    
    if (i < BUZZER_ANZAHL_TOENE - 1) {   // Pause nur zwischen Tönen, nicht nach dem letzten
      safeDelay(BUZZER_PAUSE_DAUER);     // 0,2 Sekunden Pause
    }
  }
  
  Serial.println("→ Buzzer-Warnung beendet");
}

// ============================================================================
// FANFARENSIGNAL (AKTIVER BUZZER)
// ============================================================================
void fanfare_abspielen() {
  // Fanfarensignal: 5 schnelle Töne
  Serial.println("→ Fanfarensignal...");
  
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);  // Buzzer EIN
    safeDelay(300);                 // 0,3 Sekunden Ton
    digitalWrite(BUZZER_PIN, LOW);   // Buzzer AUS
    safeDelay(100);                 // 0,1 Sekunden Pause
  }
  
  Serial.println("→ Fanfarensignal beendet");
}

// ============================================================================
// HILFSFUNKTIONEN FÜR ZEITAUSGABE
// ============================================================================
void zeige_aktuelle_zeit() {
  DateTime jetzt = rtc.now();
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
// ZEITHELFER: Watchdog-freundliche Verzögerung
// ============================================================================
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
#ifdef __AVR__
    wdt_reset();
#endif
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
