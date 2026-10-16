// motor.ino – Motorsteuerung, Relais, ENA
#include <Arduino.h>
#include <AccelStepper.h>

// Pins gem. README
#define PUL_PIN 2
#define DIR_PIN 3
#define ENA_PIN 5
#define BUZZER_PIN 6   // aktiv (HIGH=Ton)
#define RELAY_PIN 8
// HW-482 Relais-Module - ACHTUNG: Manche Varianten sind ACTIVE-HIGH!
static const bool RELAY_ACTIVE_HIGH = true; // true: HIGH=an, LOW=aus / false: LOW=an, HIGH=aus

// HINWEIS: OPTO am DM320T ist die gemeinsame Versorgung für die Optokoppler.
// In Common-Cathode-Konfiguration liegt OPTO an GND. Die Steuersignale PUL+/DIR+/ENA+
// sind mit den Arduino-Pins verbunden und werden mit HIGH (+5V) aktiv.
// Bei Common-Anode muss OPTO an 5V und die PUL-/DIR-/ENA- Pins an Arduino – dann
// ENABLE_ACTIVE_HIGH und INVERT_STEP anpassen (siehe README).

// Treiber-Enable-Logik und Step/Dir-Polarität (siehe README)
// Standard: Common-Cathode (OPTO an GND, PUL+/DIR+/ENA+ an Arduino, PUL-/DIR-/ENA- an GND)
static const bool ENABLE_ACTIVE_HIGH = true; // true: ENA=HIGH aktiviert; false: ENA=LOW aktiviert
static const bool INVERT_DIR = false;        // true invertiert DIR
static const bool INVERT_STEP = false;       // true invertiert STEP-Puls

// Hilfsfunktionen jetzt NACH den Definitionen, damit Pins/Flags bekannt sind
static inline void relaySet(bool on) {
  digitalWrite(RELAY_PIN, (RELAY_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH)));
  Serial.print(F("RELAY: "));
  Serial.println(on ? F("EIN") : F("AUS"));
}

// Interne Stepper-Instanz (Treiber/Step/Dir)
static AccelStepper stepper(AccelStepper::DRIVER, PUL_PIN, DIR_PIN);

// Vorwärtsdeklarationen aus main.ino (Defaults)
extern const int FEED_STEPS_PER_SEC; // konstante Schrittfrequenz für zeitbasierte Fütterung

void motorInit() {
  // WICHTIG: Erst Wert setzen, dann als OUTPUT definieren (verhindert Glitches)
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_HIGH ? LOW : HIGH);  // Relais AUS
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(ENA_PIN, ENABLE_ACTIVE_HIGH ? LOW : HIGH);
  
  // Dann erst als OUTPUT setzen
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Polaritäten für Step/Dir/Enable setzen (AccelStepper-intern)
  stepper.setPinsInverted(INVERT_DIR, INVERT_STEP, !ENABLE_ACTIVE_HIGH);
  // Basiswerte; tatsächliche Feed-Geschwindigkeit wird vor jedem Lauf gesetzt
  stepper.setMaxSpeed(4000.0);
  stepper.setAcceleration(8000.0);
  stepper.setMinPulseWidth(8);      // Min. Pulsbreite in µs (DM320T fordert ≥7.5µs)
  // Startposition definieren
  stepper.setCurrentPosition(0);
}

void motorEnable(bool on) {
  digitalWrite(ENA_PIN, ENABLE_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
}

void motorReset() {
  // Sicherer Stopp und definierter Zustand
  stepper.stop();
  stepper.setSpeed(0);
  stepper.setCurrentPosition(0);
  relaySet(false);
  motorEnable(false);
}

// Führt eine Fütterung aus (blocking Platzhalter). Rückgabe true bei Erfolg.
// Spielt zusätzlich einmal das Jagdsignal "Zum Essen" ab - unabhängig von der Motordauer.
bool motorFeed(int steps, bool dirCW, bool playSignal) {
  Serial.print(F("motorFeed START: steps="));
  Serial.print(steps);
  Serial.print(F(" dir="));
  Serial.println(dirCW ? F("CW") : F("CCW"));

  // Jagdsignal "Zum Essen" - Rhythmus im 3/8-Takt
  // "Kommt doch her-bei (x2), Jä-ger, Trei-ber, kommt doch her-bei (x2)!
  //  Es-sen gibt's jetzt! Erb-sen-sup-pe mit fet-tem Schwei-ne-bauch,
  //  Erb-sen-sup-pe, Schnaps gibt's auch."
  // Achtel=200ms Ton + 40ms Gap, Viertel=400ms, Komma-Pause=335ms, End-Pause=2700ms
  // Format: Ton(HIGH)-Gap(LOW)-Ton-Gap-...
  static const uint16_t PHASE_DURATIONS[] = {
    // "Kommt doch her-bei," (4 Achtel + Komma)
    200, 40, 200, 40, 200, 40, 200, 335,
    // "kommt doch her-bei," (4 Achtel + Komma)
    200, 40, 200, 40, 200, 40, 200, 335,
    // "Jä-ger," (2 Achtel + Komma)
    200, 40, 200, 335,
    // "Trei-ber," (2 Achtel + Komma)
    200, 40, 200, 335,
    // "kommt doch her-bei," (4 Achtel + Komma)
    200, 40, 200, 40, 200, 40, 200, 335,
    // "kommt doch her-bei!" (4 Achtel + lange Pause)
    200, 40, 200, 40, 200, 40, 200, 535,
    // "Es-sen gibt's" (3 Achtel)
    200, 40, 200, 40, 200, 110,
    // "jetzt!" (Viertel + Pause)
    400, 535,
    // "Erb-sen-sup-pe mit fet-tem Schwei-ne-bauch," (9 Achtel + Komma)
    200, 40, 200, 40, 200, 40, 200, 40, 200, 40, 200, 40, 200, 40, 200, 40, 200, 335,
    // "Erb-sen-sup-pe," (4 Achtel + Komma)
    200, 40, 200, 40, 200, 40, 200, 335,
    // "Schnaps gibt's" (2 Achtel gebunden)
    200, 40, 400, 110,
    // "auch." (Viertel + End-Pause)
    400, 2700
  };
  static const uint8_t PHASE_STATES[] = {
    HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW,
    HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW, HIGH, LOW, HIGH, LOW,
    HIGH, LOW, HIGH, LOW,
    HIGH, LOW
  };
  const uint8_t PHASE_COUNT = sizeof(PHASE_DURATIONS) / sizeof(PHASE_DURATIONS[0]);

  // 1) Zeitprüfung erfolgt außerhalb (Scheduler)
  // 2) Relais EIN
  relaySet(true);
  delay(200); // Relais anlaufen lassen

  // 3) Motor starten
  // Richtung wird von AccelStepper über setPinsInverted/DIR-Pin gesetzt
  Serial.print(F("Motor ENA wird aktiviert..."));
  motorEnable(true);
  delay(50); // Enable-Setup-Zeit
  Serial.println(F(" OK"));
  // Zeitbasierte konstante Geschwindigkeit:  FEED_STEPS_PER_SEC
  long delta = (long)steps * (dirCW ? 1 : -1);
  // Absolute Zielposition relativ zur aktuellen Position
  long targetPos = stepper.currentPosition() + delta;
  stepper.moveTo(targetPos);
  stepper.setSpeed(delta >= 0 ? (float)FEED_STEPS_PER_SEC : -(float)FEED_STEPS_PER_SEC);
  // Watchdog: erwartete Laufzeit + Reserve (ms)
  unsigned long sps = (unsigned long)(FEED_STEPS_PER_SEC > 0 ? FEED_STEPS_PER_SEC : 1000);
  unsigned long expectedMs = (unsigned long)(((unsigned long)abs(steps) * 1000UL) / (sps ? sps : 1UL));
  unsigned long deadline = millis() + expectedMs + 4000UL; // 4s Reserve
  unsigned long lastWdReset = millis();

  // Jagdsignal startet sofort mit dem ersten Ton
  unsigned long startMs = millis();
  unsigned long signalMs = 0;
  if (playSignal) {
    for (uint8_t i = 0; i < PHASE_COUNT; i++) signalMs += (unsigned long)PHASE_DURATIONS[i];
  }
  uint8_t phaseIndex = playSignal ? 0 : PHASE_COUNT;
  unsigned long lastBuzzerToggle = startMs;
  if (playSignal && PHASE_COUNT > 0) {
    digitalWrite(BUZZER_PIN, PHASE_STATES[0]);
    lastBuzzerToggle += (unsigned long)PHASE_DURATIONS[0];
    phaseIndex = 1;
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
  if (playSignal) {
    Serial.print(F("Jagdsignal laeuft, Dauer="));
    Serial.print(signalMs);
    Serial.println(F("ms"));
  }

  Serial.print(F("Motor startet. Ziel: "));
  Serial.print(targetPos);
  Serial.print(F(" Steps. Distance: "));
  Serial.println(stepper.distanceToGo());

  bool motorTimeout = false;
  // Motor + Signal parallel; Timeout gilt für den Motor
  while (stepper.distanceToGo() != 0) {
    stepper.runSpeedToPosition();

    // Jagdsignal unabhängig vom Motor weiterschalten
    if (phaseIndex < PHASE_COUNT) {
      if ((long)(millis() - lastBuzzerToggle) >= 0) {
        digitalWrite(BUZZER_PIN, PHASE_STATES[phaseIndex]);
        lastBuzzerToggle += (unsigned long)PHASE_DURATIONS[phaseIndex];
        phaseIndex++;
      }
    }

    // Watchdog alle 2 Sekunden zurücksetzen während Motorlauf
    #ifdef __AVR__
      if (millis() - lastWdReset > 2000) {
        wdt_reset();
        lastWdReset = millis();
      }
    #endif
    if ((long)(millis() - deadline) > 0) {
      Serial.println(F("TIMEOUT: Sicherheitsabbruch!"));
      motorTimeout = true;
      break; // Sicherheitsabbruch
    }
  }

  // Signal ggf. nach Motorlauf zu Ende spielen
  while (phaseIndex < PHASE_COUNT) {
    if ((long)(millis() - lastBuzzerToggle) >= 0) {
      digitalWrite(BUZZER_PIN, PHASE_STATES[phaseIndex]);
      lastBuzzerToggle += (unsigned long)PHASE_DURATIONS[phaseIndex];
      phaseIndex++;
    }
    #ifdef __AVR__
      if (millis() - lastWdReset > 2000) {
        wdt_reset();
        lastWdReset = millis();
      }
    #endif
  }

  digitalWrite(BUZZER_PIN, LOW); // Buzzer sicher aus

  unsigned long elapsedMs = millis() - startMs;
  bool success = (stepper.distanceToGo() == 0 && !motorTimeout);
  if (success) {
    Serial.print(F("Motor fertig nach "));
    Serial.print(elapsedMs);
    Serial.println(F("ms"));
  } else {
    Serial.println(F("TIMEOUT: Motorlauf unvollständig"));
  }

  // 5) Nachlauf
  delay(200);
  // 6) Relais AUS, Treiber AUS
  relaySet(false);
  motorEnable(false);
  Serial.println(F("motorFeed ENDE"));
  return success;
}
