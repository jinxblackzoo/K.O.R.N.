#include <AccelStepper.h>

// Define the pins connected to the motor driver and relay
#define PUL_PIN 2
#define DIR_PIN 3
#define OPTO_PIN 4
#define ENA_PIN 5
#define RELAY_PIN 8

// Create an instance of the AccelStepper class
AccelStepper stepper(AccelStepper::DRIVER, PUL_PIN, DIR_PIN);

// Define variables for stepper parameters
int acceleration = 100; // Acceleration in steps per second per second
int targetRPM = 100;    // Target speed in RPM
int stepsToRun = 6000;  // Number of steps the stepper should run
const long powerOffDelay = 5000; // Delay in milliseconds to power off the stepper after each run (5 seconds)
const int relayDelay = 2000; // Delay in milliseconds between activating relay and starting stepper motor

// Define variables to track time
unsigned long previousMillis = 0;
const long initialDelay = 10 * 1000;  // 10 seconds in milliseconds
const long repeatInterval = 24 * 60 * 60 * 1000; // 24 hours in milliseconds

bool initialMovementDone = false;
bool relayActivated = false;

void setup() {
  // Set the maximum speed based on target RPM
  stepper.setMaxSpeed(targetRPM * 200); // 200 steps per revolution

  // Set the acceleration
  stepper.setAcceleration(acceleration);

  // Set the enable pin as an output and disable the stepper initially
  pinMode(ENA_PIN, OUTPUT);
  digitalWrite(ENA_PIN, LOW);

  // Set the relay pin as an output
  pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if the initial movement delay has passed and initial movement is not done
  if (!initialMovementDone && currentMillis - previousMillis >= initialDelay) {
    // Activate the relay if it's not already activated
    if (!relayActivated) {
      digitalWrite(RELAY_PIN, HIGH);
      relayActivated = true;
      delay(relayDelay); // Delay before starting the stepper motor
    }

    // Enable the stepper
    digitalWrite(ENA_PIN, HIGH);

    // Move the stepper motor the specified number of steps counterclockwise
    stepper.moveTo(-stepsToRun);
    stepper.runToPosition();

    // Disable the stepper
    digitalWrite(ENA_PIN, LOW);

    // Deactivate the relay
    digitalWrite(RELAY_PIN, LOW);

    // Set initial movement flag
    initialMovementDone = true;

    // Update previousMillis for the repeat interval
    previousMillis = currentMillis;
  }

  // Check if 24 hours have passed after the initial movement
  if (initialMovementDone && currentMillis - previousMillis >= repeatInterval) {
    // Activate the relay if it's not already activated
    if (!relayActivated) {
      digitalWrite(RELAY_PIN, HIGH);
      relayActivated = true;
      delay(relayDelay); // Delay before starting the stepper motor
    }

    // Enable the stepper
    digitalWrite(ENA_PIN, HIGH);

    // Move the stepper motor the specified number of steps counterclockwise
    stepper.moveTo(-stepsToRun);
    stepper.runToPosition();

    // Disable the stepper
    digitalWrite(ENA_PIN, LOW);

    // Delay to keep the stepper powered off for a specified duration
    delay(powerOffDelay);

    // Deactivate the relay
    digitalWrite(RELAY_PIN, LOW);

    // Update previousMillis for the next repeat interval
    previousMillis = currentMillis;
  }
}