/*
 * K.O.R.N. - Katastrophal Organisierter Nahrungsmittel Spender
 * Arduino-Code mit DS3231 Echtzeituhr für präzise Fütterungszeiten
 * 
 * VERDRAHTUNG DS3231:
 * DS3231 VCC  → Arduino 3.3V (oder 5V)
 * DS3231 GND  → Arduino GND
 * DS3231 SDA  → Arduino A4 (SDA)
 * DS3231 SCL  → Arduino A5 (SCL)
 * DS3231 SQW  → NICHT VERWENDET (Pin 2 wird für Motor verwendet)
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
#include <avr/wdt.h>  // Watchdog Timer für Fehlerabsicherung

// ============================================================================
// KONFIGURATION - HIER ALLE PARAMETER EINSTELLEN
// ============================================================================

// FÜTTERUNGSZEITEN (24h Format)
<<<<<<< HEAD:main/KORN-Motorsteuerung_simpel/KORN-Motorsteuerung_simpel.ino
const int FUETTERUNG_STUNDE_1 = 7;   // Erste Fütterung 
const int FUETTERUNG_MINUTE_1 = 0;
const int FUETTERUNG_STUNDE_2 = 16;   // Zweite Fütterung 
const int FUETTERUNG_MINUTE_2 = 0;
=======
const int FUETTERUNG_STUNDE_1 = 17;   // Erste Fütterung um 16:58 Uhr
const int FUETTERUNG_MINUTE_1 = 13;
const int FUETTERUNG_STUNDE_2 = 19;   // Zweite Fütterung um 19:00 Uhr  
const int FUETTERUNG_MINUTE_2 = 2;
>>>>>>> 624d582 (chore(repo): Verzeichnisstruktur neu organisiert):KORN_Rev3/main/KORN-Motorsteuerung_simpel/KORN-Motorsteuerung_simpel.ino

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

// WATCHDOG-TIMER KONFIGURATION
const bool WATCHDOG_AKTIV = true;     // true = Watchdog-Timer aktiviert (Schutz vor Hängern)
const int WATCHDOG_TIMEOUT = 8;       // Watchdog-Timeout in Sekunden (1, 2, 4, 8 möglich)

// DEBUG-MODUS KONFIGURATION
const bool DEBUG_MODUS = true;        // true = Detaillierte Debug-Ausgaben aktiviert
const bool DEBUG_ZEIT = false;        // true = Zusätzliche Zeit-Debug-Infos
const bool DEBUG_MOTOR = true;        // true = Motor-Debug-Ausgaben aktiviert
const bool DEBUG_BUZZER = false;      // true = Buzzer-Debug-Ausgaben aktiviert
const bool ZEIGE_TEMPERATUR = false;  // true = RTC-Temperatur in Zeitausgabe anzeigen

// ============================================================================
// PIN-DEFINITIONEN
// ============================================================================
#define PUL_PIN 2    // Schrittimpulse zum Motortreiber
#define DIR_PIN 3    // Drehrichtung zum Motortreiber  
#define OPTO_PIN 4   // Optokoppler (Reserve)
#define ENA_PIN 5    // Motor aktivieren/deaktivieren
#define BUZZER_PIN 6 // Aktiver Buzzer für Fütterungswarnung (HIGH/LOW)
#define RELAY_PIN 8  // Relais für Motortreiber-Stromversorgung

// MANUELLE AUSLÖSUNG (Kurzschluss zwischen Pin 10 und 11)
#define MANUAL_TRIGGER_PIN 10  // INPUT_PULLUP (normalerweise HIGH)
#define MANUAL_GROUND_PIN 11   // OUTPUT LOW (immer LOW für Kurzschluss)

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

// Non-blocking Timer-Variablen für State-Machine
enum FuetterungsZustand {
  BEREIT,
  BUZZER_WARNUNG,
  BUZZER_PAUSE,
  WARTE_NACH_BUZZER,
  RELAIS_AKTIVIERT,
  MOTOR_LAEUFT,
  WARTE_NACH_MOTOR
};

FuetterungsZustand aktueller_zustand = BEREIT;
unsigned long timer_start = 0;
unsigned long letzter_zeitcheck = 0;
int buzzer_ton_counter = 0;
bool buzzer_aktiv = false;

// Variablen für manuelle Auslösung
bool letzter_manual_zustand = HIGH;    // Letzter Zustand des Manual-Pins
unsigned long letzter_manual_zeit = 0; // Zeitpunkt der letzten Zustandsänderung
const unsigned long ENTPRELL_ZEIT = 50; // Entprellzeit in Millisekunden

// ============================================================================
// DEBUG-HELPER-FUNKTIONEN
// ============================================================================
void debug_print(const __FlashStringHelper* nachricht) {
  if (DEBUG_ZEIT) {
    Serial.println(nachricht);
  }
}

void debug_print_timing(const __FlashStringHelper* nachricht) {
  if (DEBUG_ZEIT) {
    Serial.print(F("[T]"));
    Serial.println(nachricht);
  }
}

void debug_print_motor(const __FlashStringHelper* nachricht) {
  if (DEBUG_MOTOR) {
    Serial.print(F("[M]"));
    Serial.println(nachricht);
  }
}

void debug_print_buzzer(const __FlashStringHelper* nachricht) {
  if (DEBUG_BUZZER) {
    Serial.print(F("[B]"));
    Serial.println(nachricht);
  }
}

// Konsolidierte Status-Ausgabe
void zeige_system_status() {
  Serial.println(F("STATUS:"));
  Serial.print(F("DBG:")); Serial.println(DEBUG_ZEIT ? F("ON") : F("OFF"));
  Serial.print(F("WDT:")); Serial.println(WATCHDOG_AKTIV ? F("ON") : F("OFF"));
  Serial.print(F("SYNC:")); Serial.println(ZEIT_EINSTELLEN ? F("ON") : F("OFF"));
  Serial.print(F("WARN:")); Serial.println(BUZZER_VORWARNUNG);
  Serial.print(F("STEPS:")); Serial.println(MOTOR_SCHRITTE);
}

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
  Serial.println(F("K.O.R.N. INIT"));
  
  // RTC Init
  Serial.println(F("RTC..."));
  
  bool rtc_ok = false;
  int tries = 0;
  
  while (!rtc_ok && tries < 3) {
    tries++;
    Serial.print(F("Try ")); Serial.println(tries);
    
    if (rtc.begin()) {
      DateTime now = rtc.now();
      
      if (now.year() >= 2023 && now.year() <= 2030) {
        Serial.println(F("RTC OK"));
        rtc_ok = true;
        
        Serial.print(now.day()); Serial.print(F("."));
        Serial.print(now.month()); Serial.print(F("."));
        Serial.print(now.year()); Serial.print(F(" "));
        Serial.print(now.hour()); Serial.print(F(":"));
        Serial.print(now.minute()); Serial.print(F(":"));
        Serial.println(now.second());
        
        if (ZEIGE_TEMPERATUR) {
          Serial.print(F("T:")); Serial.println(rtc.getTemperature());
        }
      } else {
        Serial.println(F("RTC time bad"));
        rtc_ok = true;
      }
    } else {
      Serial.println(F("RTC not found"));
      if (tries < 3) delay(2000);
    }
  }
  
  // Endgültige Fehlerbehandlung
  if (!rtc_ok) {
    Serial.println(F("RTC FAIL!"));
    Serial.println(F("NOTFALL-MODUS"));
    Serial.println(F("Nur manuell!"));
    
    // Warnsignal
    for (int i = 0; i < 10; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
    
    // System läuft weiter, aber ohne Zeitfunktionen
    return;  // setup() beenden ohne weitere Initialisierung
  }
  
  // Zeit und Datum einstellen (automatisch bei jedem Upload)
  // WICHTIG: Da Arduino keine Internet-Zeitsynchronisation hat, ist dies die einzige
  // Möglichkeit, die RTC präzise zu halten. Compile-Zeit ist die verfügbare Zeitquelle.
  if (ZEIT_EINSTELLEN) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println(F("Time sync OK"));
    DateTime jetzt = rtc.now();
    Serial.print(jetzt.day()); Serial.print(F("."));
    Serial.print(jetzt.month()); Serial.print(F("."));
    Serial.print(jetzt.year()); Serial.print(F(" "));
    Serial.print(jetzt.hour()); Serial.print(F(":"));
    Serial.print(jetzt.minute()); Serial.print(F(":"));
    Serial.println(jetzt.second());
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
  
  // Manuelle Auslösung konfigurieren
  pinMode(MANUAL_TRIGGER_PIN, INPUT_PULLUP);  // Pin 10: Normalerweise HIGH
  pinMode(MANUAL_GROUND_PIN, OUTPUT);         // Pin 11: Immer LOW
  digitalWrite(MANUAL_GROUND_PIN, LOW);       // Pin 11 auf LOW setzen
  
  // Motor und Relais initial ausschalten
  digitalWrite(ENA_PIN, LOW);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.println(F("Manual: Pin 10+11"));
  fanfare_abspielen();
  Serial.print(F("Feed: ")); Serial.print(FUETTERUNG_STUNDE_1); Serial.print(F(":")); Serial.print(FUETTERUNG_MINUTE_1);
  Serial.print(F("/")); Serial.print(FUETTERUNG_STUNDE_2); Serial.print(F(":")); Serial.println(FUETTERUNG_MINUTE_2);
  
  // Watchdog
  if (WATCHDOG_AKTIV) {
    Serial.print(F("WDT:"));
    wdt_disable();
    wdt_reset();
    
    #if WATCHDOG_TIMEOUT == 1
      wdt_enable(WDTO_1S);
      Serial.println(F("1s"));
    #elif WATCHDOG_TIMEOUT == 2
      wdt_enable(WDTO_2S);
      Serial.println(F("2s"));
    #elif WATCHDOG_TIMEOUT == 4
      wdt_enable(WDTO_4S);
      Serial.println(F("4s"));
    #elif WATCHDOG_TIMEOUT == 8
      wdt_enable(WDTO_8S);
      Serial.println(F("8s"));
    #endif
  }
  
  zeige_system_status();
  Serial.println(F("READY"));
}

void loop() {
  // Watchdog-Timer regelmäßig zurücksetzen (verhindert Neustart)
  if (WATCHDOG_AKTIV) {
    wdt_reset();
  }
  
  DateTime jetzt = rtc.now();
  unsigned long aktuelle_zeit = millis();
  
  // Manuelle Auslösung prüfen (Pin 10 und 11 kurzschließen)
  bool aktueller_manual_zustand = digitalRead(MANUAL_TRIGGER_PIN);
  
  // Entprellung: Nur bei stabiler Zustandsänderung reagieren
  if (aktueller_manual_zustand != letzter_manual_zustand) {
    if (aktuelle_zeit - letzter_manual_zeit > ENTPRELL_ZEIT) {
      letzter_manual_zustand = aktueller_manual_zustand;
      letzter_manual_zeit = aktuelle_zeit;
      
      // Manuelle Auslösung bei LOW (Kurzschluss) und System bereit
      if (aktueller_manual_zustand == LOW && aktueller_zustand == BEREIT) {
        Serial.println(F("MANUAL FEED"));
        starte_fuetterung();
      }
    }
  }
  
  // Zeitausgabe alle 30 Sekunden (non-blocking)
  if (aktuelle_zeit - letzter_zeitcheck >= ZEIT_AUSGABE_INTERVALL * 1000) {
    if (jetzt.second() != letzte_sekunde && jetzt.second() % 30 == 0) {
      zeige_aktuelle_zeit();
      letzte_sekunde = jetzt.second();
      letzter_zeitcheck = aktuelle_zeit;
    }
  }
  
  // Tagesreset um Mitternacht - Fütterungsflags zurücksetzen
  if (jetzt.day() != letzter_tag) {
    fuetterung1_heute = false;
    fuetterung2_heute = false;
    warnung1_heute = false;
    warnung2_heute = false;
    letzter_tag = jetzt.day();
    
    // Neues Datum anzeigen
    Serial.print(F("NEW DAY: "));
    Serial.print(jetzt.day());
    Serial.print(F("."));
    Serial.print(jetzt.month());
    Serial.print(F("."));
    Serial.print(jetzt.year());
    Serial.println(F(" Reset"));
  }
  
  // Erste Fütterungszeit prüfen - FÜTTERUNG
  if (!fuetterung1_heute && aktueller_zustand == BEREIT &&
      jetzt.hour() == FUETTERUNG_STUNDE_1 && 
      jetzt.minute() == FUETTERUNG_MINUTE_1) {
    
    Serial.println(F("FEED 1"));
    starte_fuetterung();
    fuetterung1_heute = true;
  }
  
  // Zweite Fütterungszeit prüfen - FÜTTERUNG
  if (!fuetterung2_heute && aktueller_zustand == BEREIT &&
      jetzt.hour() == FUETTERUNG_STUNDE_2 && 
      jetzt.minute() == FUETTERUNG_MINUTE_2) {
    
    Serial.println(F("FEED 2"));
    starte_fuetterung();
    fuetterung2_heute = true;
  }
  
  // Non-blocking Fütterungsvorgang abarbeiten
  fuetterungsvorgang_non_blocking();
}

// ============================================================================
// NON-BLOCKING FÜTTERUNGSVORGANG - State Machine
// ============================================================================
void starte_fuetterung() {
  aktueller_zustand = BUZZER_WARNUNG;
  timer_start = millis();
  buzzer_ton_counter = 0;
  buzzer_aktiv = false;
  Serial.println(F("WARN"));
}

void fuetterungsvorgang_non_blocking() {
  unsigned long aktuelle_zeit = millis();
  
  switch (aktueller_zustand) {
    case BEREIT:
      // Warten auf Fütterungszeit
      break;
      
    case BUZZER_WARNUNG:
      // Buzzer-Warnung non-blocking
      if (buzzer_ton_counter < BUZZER_ANZAHL_TOENE) {
        if (!buzzer_aktiv) {
          // Neuen Ton starten
          Serial.print("  Ton "); Serial.print(buzzer_ton_counter + 1); Serial.println("...");
          digitalWrite(BUZZER_PIN, HIGH);
          buzzer_aktiv = true;
          timer_start = aktuelle_zeit;
        } else if (aktuelle_zeit - timer_start >= BUZZER_TON_DAUER) {
          // Ton beenden
          digitalWrite(BUZZER_PIN, LOW);
          buzzer_aktiv = false;
          buzzer_ton_counter++;
          timer_start = aktuelle_zeit;
          
          // Pause zwischen Tönen (außer nach letztem Ton)
          if (buzzer_ton_counter < BUZZER_ANZAHL_TOENE) {
            aktueller_zustand = BUZZER_PAUSE;
          }
        }
      } else {
        // Buzzer-Warnung beendet
        Serial.println(F("WARN END"));
        aktueller_zustand = WARTE_NACH_BUZZER;
        timer_start = aktuelle_zeit;
        Serial.print(F("WAIT "));
        Serial.print(BUZZER_VORWARNUNG / 1000);
        Serial.println(F("s"));
      }
      break;
      
    case BUZZER_PAUSE:
      // Non-blocking Pause zwischen Buzzer-Tönen
      if (aktuelle_zeit - timer_start >= BUZZER_PAUSE_DAUER) {
        aktueller_zustand = BUZZER_WARNUNG;
        timer_start = aktuelle_zeit;
      }
      break;
      
    case WARTE_NACH_BUZZER:
      if (aktuelle_zeit - timer_start >= BUZZER_VORWARNUNG * 1000) {
        aktueller_zustand = RELAIS_AKTIVIERT;
        timer_start = aktuelle_zeit;
        Serial.println(F("REL ON"));
        digitalWrite(RELAY_PIN, HIGH);
      }
      break;
      
    case RELAIS_AKTIVIERT:
      if (aktuelle_zeit - timer_start >= RELAIS_VERZOEGERUNG) {
        aktueller_zustand = MOTOR_LAEUFT;
        timer_start = aktuelle_zeit;
        Serial.println(F("MOT ON"));
        digitalWrite(ENA_PIN, HIGH);
        
        Serial.print(F("RUN "));
        Serial.print(MOTOR_SCHRITTE);
        Serial.println(F(" steps"));
        
        if (MOTOR_RECHTS) {
          stepper.moveTo(MOTOR_SCHRITTE);
        } else {
          stepper.moveTo(-MOTOR_SCHRITTE);
        }
      }
      break;
      
    case MOTOR_LAEUFT:
      // Non-blocking Motor-Bewegung
      if (stepper.distanceToGo() != 0) {
        stepper.run();  // Ein Schritt pro Loop-Durchgang
      } else {
        // Motor-Bewegung beendet
        Serial.println(F("MOT OFF"));
        digitalWrite(ENA_PIN, LOW);
        stepper.setCurrentPosition(0);  // Position zurücksetzen
        aktueller_zustand = WARTE_NACH_MOTOR;
        timer_start = aktuelle_zeit;
      }
      break;
      
    case WARTE_NACH_MOTOR:
      if (aktuelle_zeit - timer_start >= MOTOR_PAUSE) {
        Serial.println(F("REL OFF"));
        digitalWrite(RELAY_PIN, LOW);
        aktueller_zustand = BEREIT;
        Serial.println(F("DONE"));
      }
      break;
  }
}

// ============================================================================
// LEGACY FÜTTERUNGSVORGANG - Motor ansteuern (BLOCKIEREND - NUR FÜR FALLBACK)
// ============================================================================
void fuetterungsvorgang() {
  // 🔊 BUZZER-WARNUNG: 3x Beep für je 1 Sekunde
  Serial.println(F("WARN"));
  buzzer_warnung();
  
  // Vorwarnzeit warten nach Buzzer-Warnung
  Serial.print(F("WAIT "));
  Serial.print(BUZZER_VORWARNUNG / 1000);
  Serial.println(F("s"));
  delay(BUZZER_VORWARNUNG * 1000);
  
  Serial.println(F("REL ON"));
  digitalWrite(RELAY_PIN, HIGH);           // Motortreiber mit Strom versorgen
  delay(RELAIS_VERZOEGERUNG);              // Warten bis Treiber bereit
  
  Serial.println(F("MOT ON"));
  digitalWrite(ENA_PIN, HIGH);             // Motor aktivieren
  
  Serial.print(F("RUN "));
  Serial.print(MOTOR_SCHRITTE);
  Serial.println(F(" steps"));
  
  if (MOTOR_RECHTS) {
    stepper.moveTo(MOTOR_SCHRITTE);         // Bewegung im Uhrzeigersinn
  } else {
    stepper.moveTo(-MOTOR_SCHRITTE);        // Bewegung gegen Uhrzeigersinn
  }
  stepper.runToPosition();                 // Bewegung ausführen (blockierend)
  
  Serial.println(F("MOT OFF"));
  digitalWrite(ENA_PIN, LOW);              // Motor deaktivieren
  stepper.setCurrentPosition(0);           // Position zurücksetzen für nächste Bewegung
  
  delay(MOTOR_PAUSE);                      // Kurze Pause
  
  Serial.println(F("REL OFF"));
  digitalWrite(RELAY_PIN, LOW);            // Motortreiber stromlos schalten
}

// ============================================================================
// BUZZER-WARNUNG (AKTIVER BUZZER)
// ============================================================================
void buzzer_warnung() {
  Serial.println(F("BUZZ 3x"));
  
  for (int i = 0; i < BUZZER_ANZAHL_TOENE; i++) {
    Serial.print(F("T")); Serial.println(i + 1);
    
    // Aktiven Buzzer mit HIGH-Signal aktivieren
    digitalWrite(BUZZER_PIN, HIGH);      // Buzzer EIN
    delay(BUZZER_TON_DAUER);             // 1 Sekunde Ton
    digitalWrite(BUZZER_PIN, LOW);       // Buzzer AUS
    
    if (i < BUZZER_ANZAHL_TOENE - 1) {   // Pause nur zwischen Tönen, nicht nach dem letzten
      delay(BUZZER_PAUSE_DAUER);         // 0,2 Sekunden Pause
    }
  }
  
  Serial.println(F("BUZZ END"));
}

// ============================================================================
// FANFARENSIGNAL (AKTIVER BUZZER)
// ============================================================================
void fanfare_abspielen() {
  // Fanfarensignal: 5 schnelle Töne
  Serial.println(F("FANFARE"));
  
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);  // Buzzer EIN
    delay(300);                     // 0,3 Sekunden Ton
    digitalWrite(BUZZER_PIN, LOW);   // Buzzer AUS
    delay(100);                     // 0,1 Sekunden Pause
  }
  
  Serial.println(F("FANFARE END"));
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
  
  // Temperatur vom DS3231 anzeigen (optional)
  if (ZEIGE_TEMPERATUR) {
    Serial.print(" | Temp: ");
    Serial.print(rtc.getTemperature());
    Serial.println("°C");
  } else {
    Serial.println();
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
