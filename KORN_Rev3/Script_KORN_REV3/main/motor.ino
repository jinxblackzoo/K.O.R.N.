// motor.ino – Motorsteuerung, Relais, ENA
#include <Arduino.h>
#include <AccelStepper.h>

// Pins gem. README
#define PUL_PIN 2
#define DIR_PIN 3
#define ENA_PIN 5
#define BUZZER_PIN 6   // aktiv (HIGH=Ton)
#define RELAY_PIN 8
// Einige Relais-Module sind ACTIVE-LOW. Bei wildem Klacken ggf. auf false setzen.
static const bool RELAY_ACTIVE_HIGH = true; // true: HIGH=an, LOW=aus; false: LOW=an, HIGH=aus
#define OPTO_PIN 4     // Reserve/OPTO

// Treiber-Enable-Logik und Step/Dir-Polarität (falls Motor nur hält, hier anpassen)
static const bool ENABLE_ACTIVE_HIGH = true; // true: ENA=HIGH aktiviert; false: ENA=LOW aktiviert
static const bool INVERT_DIR = false;        // true invertiert DIR
static const bool INVERT_STEP = false;       // true invertiert STEP-Puls

// Hilfsfunktionen jetzt NACH den Definitionen, damit Pins/Flags bekannt sind
static inline void relaySet(bool on) {
  digitalWrite(RELAY_PIN, (RELAY_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH)));
}

static void buzzerBeep(uint16_t ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// Interne Stepper-Instanz (Treiber/Step/Dir)
static AccelStepper stepper(AccelStepper::DRIVER, PUL_PIN, DIR_PIN);

// Vorwärtsdeklarationen aus main.ino (Defaults)
extern const int MOTOR_SCHRITTE;
extern const int FEED_STEPS_PER_SEC; // konstante Schrittfrequenz für zeitbasierte Fütterung

void motorInit() {
  pinMode(PUL_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(OPTO_PIN, INPUT);

  // Treiber aus (gemäß ENABLE_ACTIVE_HIGH)
  digitalWrite(ENA_PIN, ENABLE_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  // Relais sicher AUS
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_HIGH ? LOW : HIGH);

  // Polaritäten für Step/Dir/Enable setzen (AccelStepper-intern)
  stepper.setPinsInverted(INVERT_DIR, INVERT_STEP, !ENABLE_ACTIVE_HIGH);
  // Basiswerte; tatsächliche Feed-Geschwindigkeit wird vor jedem Lauf gesetzt
  stepper.setMaxSpeed(4000.0);
  stepper.setAcceleration(8000.0);
  stepper.setMinPulseWidth(8);      // Min. Pulsbreite in µs (DM320T fordert ≥7.5µs)
}

void motorEnable(bool on) {
  digitalWrite(ENA_PIN, ENABLE_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH));
}

void motorReset() {
  stepper.stop();
}

// Führt eine Fütterung aus (blocking Platzhalter). Rückgabe true bei Erfolg.
bool motorFeed(int steps, bool dirCW) {
  // 1) Zeitprüfung erfolgt außerhalb (Scheduler)
  // 2) Relais EIN
  relaySet(true);
  delay(200); // Relais anlaufen lassen
  // 3) Buzzer (kurz)
  buzzerBeep(150);
  delay(100);
  // 4) Motor starten
  digitalWrite(DIR_PIN, dirCW ? HIGH : LOW);
  motorEnable(true);
  delay(50); // Enable-Setup-Zeit
  // Zeitbasierte konstante Geschwindigkeit:  FEED_STEPS_PER_SEC
  long target = steps * (dirCW ? 1 : -1);
  stepper.move(target);
  stepper.setSpeed(dirCW ? (float)FEED_STEPS_PER_SEC : -(float)FEED_STEPS_PER_SEC);
  while (stepper.distanceToGo() != 0) {
    stepper.runSpeedToPosition();
  }
  // 5) Nachlauf
  delay(200);
  // 6) Relais AUS, Treiber AUS
  relaySet(false);
  motorEnable(false);
  return true;
}
