/*
 * K.O.R.N. - Katastrophal Organisierter Nahrungsmittel Spender
 * Arduino-Code mit DS3231 Echtzeituhr für präzise Fütterungszeiten
 * 
 * VERDRAHTUNG DS3231:
 * DS3231 VCC  → Arduino 3.3V (oder 5V)
 * DS3231 GND  → Arduino GND
 * DS3231 SDA  → Arduino A4 (SDA)
 * DS3231 SCL  → Arduino A5 (SCL)
 * DS3231 SQW  → Arduino Pin 2 (Interrupt, optional)
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
 * Pin 6 ist PWM-fähig und wird für tone() Funktion benötigt!
 */

#include <AccelStepper.h>
#include <RTClib.h>
#include <Wire.h>

// ============================================================================
// KONFIGURATION - HIER ALLE PARAMETER EINSTELLEN
// ============================================================================

// FÜTTERUNGSZEITEN (24h Format)
const int FUETTERUNG_STUNDE_1 = 17;   // Erste Fütterung um 16:58 Uhr
const int FUETTERUNG_MINUTE_1 = 13;
const int FUETTERUNG_STUNDE_2 = 19;   // Zweite Fütterung um 19:00 Uhr  
const int FUETTERUNG_MINUTE_2 = 2;

// MOTORPARAMETER
const int MOTOR_BESCHLEUNIGUNG = 100;  // Beschleunigung in Steps/s²
const int MOTOR_RPM = 100;             // Geschwindigkeit in Umdrehungen/Minute
const int MOTOR_SCHRITTE = 6000;       // Schritte pro Fütterung (30 Umdrehungen)
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
const bool ZEIT_EINSTELLEN = true;    // true = Zeit wird bei jedem Upload automatisch korrigiert

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
bool fuetterung1_heute = false;  // Erste Fütterung heute bereits erfolgt?
bool fuetterung2_heute = false;  // Zweite Fütterung heute bereits erfolgt?
bool warnung1_heute = false;     // Warnung für erste Fütterung bereits erfolgt?
bool warnung2_heute = false;     // Warnung für zweite Fütterung bereits erfolgt?
int letzter_tag = -1;            // Letzter Tag für Tagesreset
static int letzte_sekunde = -1;  // Letzte Sekunde für Zeitausgabe

void setup() {
  // ===================
  // POWERUP-SIGNALTON: 2x 1 Sekunde mit 1 Sekunde Pause
  // ===================
  // Signalisiert, dass der Arduino frisch mit Strom versorgt wurde (Powerup)
  // 1. Buzzer 1 Sekunde AN
  digitalWrite(BUZZER_PIN, HIGH);   // Buzzer EIN
  delay(1000);                      // 1 Sekunde warten
  digitalWrite(BUZZER_PIN, LOW);    // Buzzer AUS
  delay(1000);                      // 1 Sekunde Pause
  // 2. Buzzer 1 Sekunde AN
  digitalWrite(BUZZER_PIN, HIGH);   // Buzzer EIN
  delay(1000);                      // 1 Sekunde warten
  digitalWrite(BUZZER_PIN, LOW);    // Buzzer AUS
  // ===================


  Serial.begin(9600);
  Serial.println("===========================================");
  Serial.println("🐓 K.O.R.N. Fütterungsautomat gestartet!");
  Serial.println("===========================================");
  
  // RTC initialisieren
  if (!rtc.begin()) {
    Serial.println("FEHLER: DS3231 nicht gefunden!");
    Serial.println("Prüfe Verdrahtung:");
    Serial.println("VCC → 3.3V, GND → GND, SDA → A4, SCL → A5");
    while (1) delay(1000);  // Endlosschleife bei RTC-Fehler
  }
  
  // Zeit und Datum einstellen (automatisch bei jedem Upload)
  if (ZEIT_EINSTELLEN) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("📡 DS3231 RTC erfolgreich initialisiert");
    Serial.println("⚡ Automatische Zeit- und Datum-Synchronisation aktiviert");
    
    // Synchronisiertes Datum anzeigen
    DateTime jetzt = rtc.now();
    Serial.print("📅 Datum synchronisiert: ");
    Serial.print(jetzt.day(), DEC);
    Serial.print(".");
    Serial.print(jetzt.month(), DEC);
    Serial.print(".");
    Serial.print(jetzt.year(), DEC);
    Serial.println();
  }
  
  // Aktuelle Zeit anzeigen
  zeige_aktuelle_zeit();
  
  // Motor konfigurieren
  stepper.setMaxSpeed(MOTOR_RPM * 200);        // 200 Steps = 1 Umdrehung
  stepper.setAcceleration(MOTOR_BESCHLEUNIGUNG);
  
  // Pins konfigurieren
  pinMode(ENA_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Motor und Relais initial ausschalten
  digitalWrite(ENA_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println("🎺 Upload erfolgreich - Fanfare wird abgespielt...");
  fanfare_abspielen();
  
  Serial.println("-------------------------------------------");
  Serial.println("Konfigurierte Fütterungszeiten:");
  Serial.print("1. Fütterung: "); 
  formatiere_zeit(FUETTERUNG_STUNDE_1, FUETTERUNG_MINUTE_1);
  Serial.print("2. Fütterung: ");
  formatiere_zeit(FUETTERUNG_STUNDE_2, FUETTERUNG_MINUTE_2);
  Serial.println("-------------------------------------------");
  Serial.println("System bereit! Zeitausgabe alle 30 Sekunden.");
  Serial.println("===========================================");
}

void loop() {
  DateTime jetzt = rtc.now();
  
  // Zeitausgabe alle 30 Sekunden (bei Sekundenwechsel)
  if (jetzt.second() != letzte_sekunde && jetzt.second() % 30 == 0) {
    zeige_aktuelle_zeit();
    letzte_sekunde = jetzt.second();
  }
  
  // Tagesreset um Mitternacht - Fütterungsflags zurücksetzen
  if (jetzt.day() != letzter_tag) {
    fuetterung1_heute = false;
    fuetterung2_heute = false;
    warnung1_heute = false;
    warnung2_heute = false;
    letzter_tag = jetzt.day();
    
    // Neues Datum anzeigen
    Serial.print("📅 NEUER TAG: ");
    Serial.print(jetzt.day(), DEC);
    Serial.print(".");
    Serial.print(jetzt.month(), DEC);
    Serial.print(".");
    Serial.print(jetzt.year(), DEC);
    Serial.println(" - Fütterungsflags zurückgesetzt");
  }
  
  // Erste Fütterungszeit prüfen - FÜTTERUNG
  if (!fuetterung1_heute && 
      jetzt.hour() == FUETTERUNG_STUNDE_1 && 
      jetzt.minute() == FUETTERUNG_MINUTE_1) {
    
    Serial.println("🍽️ ERSTE FÜTTERUNG STARTET...");
    fuetterungsvorgang();
    fuetterung1_heute = true;
    Serial.println("✅ Erste Fütterung abgeschlossen!");
  }
  
  // Zweite Fütterungszeit prüfen - FÜTTERUNG
  if (!fuetterung2_heute && 
      jetzt.hour() == FUETTERUNG_STUNDE_2 && 
      jetzt.minute() == FUETTERUNG_MINUTE_2) {
    
    Serial.println("🍽️ ZWEITE FÜTTERUNG STARTET...");
    fuetterungsvorgang();
    fuetterung2_heute = true;
    Serial.println("✅ Zweite Fütterung abgeschlossen!");
  }
  
  // 30 Sekunden warten bevor nächste Zeitprüfung
  // Verhindert mehrfache Fütterung in derselben Minute
  delay(30000);
}

// ============================================================================
// FÜTTERUNGSVORGANG - Motor ansteuern
// ============================================================================
void fuetterungsvorgang() {
  // 🔊 BUZZER-WARNUNG: 3x Beep für je 1 Sekunde
  Serial.println("🔊 Fütterungswarnung: 3x Beep...");
  buzzer_warnung();
  
  // 3 Sekunden warten nach Buzzer-Warnung
  Serial.println("⏱️ 3 Sekunden warten...");
  delay(3000);
  
  Serial.println("→ Relais aktivieren...");
  digitalWrite(RELAY_PIN, HIGH);           // Motortreiber mit Strom versorgen
  delay(RELAIS_VERZOEGERUNG);              // Warten bis Treiber bereit
  
  Serial.println("→ Motor aktivieren...");
  digitalWrite(ENA_PIN, HIGH);             // Motor aktivieren
  
  Serial.print("→ Motor läuft ");
  Serial.print(MOTOR_SCHRITTE);
  Serial.println(" Schritte...");
  
  if (MOTOR_RECHTS) {
    stepper.moveTo(MOTOR_SCHRITTE);         // Bewegung im Uhrzeigersinn
  } else {
    stepper.moveTo(-MOTOR_SCHRITTE);        // Bewegung gegen Uhrzeigersinn
  }
  stepper.runToPosition();                 // Bewegung ausführen (blockierend)
  
  Serial.println("→ Motor stoppen...");
  digitalWrite(ENA_PIN, LOW);              // Motor deaktivieren
  
  delay(MOTOR_PAUSE);                      // Kurze Pause
  
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
    delay(BUZZER_TON_DAUER);             // 1 Sekunde Ton
    digitalWrite(BUZZER_PIN, LOW);       // Buzzer AUS
    
    if (i < BUZZER_ANZAHL_TOENE - 1) {   // Pause nur zwischen Tönen, nicht nach dem letzten
      delay(BUZZER_PAUSE_DAUER);         // 0,2 Sekunden Pause
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
    delay(300);                     // 0,3 Sekunden Ton
    digitalWrite(BUZZER_PIN, LOW);   // Buzzer AUS
    delay(100);                     // 0,1 Sekunden Pause
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

void formatiere_zeit(int stunde, int minute) {
  if (stunde < 10) Serial.print("0");
  Serial.print(stunde);
  Serial.print(":");
  if (minute < 10) Serial.print("0");
  Serial.print(minute);
  Serial.println(" Uhr");
}
