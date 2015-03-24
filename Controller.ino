
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_PWMServoDriver.h"

// ---- Configuration
// Normalization Factors: These need to be recalibrated if lighting conditions change
const float NORMALIZATION_CONST = .33; // The amount to subtract off of each sensor value such that an empty slot = 0 opacity
const float NORMALIZATION_FACTOR = 1.65; // The amount by which to divide each sensor value such that an opaque slot = 1 opacity

// Motor Rotation Config
const float FULL_ROTATION_TIME = 19000; // Time for a full rotation in ms
const float QUARTER_ROTATION_PERIOD = FULL_ROTATION_TIME / 4;
const float SWITCH_DEBOUNCE = 5000; // Milliseconds after which the tripped switch is active again
const float MEASUREMENT_DELAY = 1000; // The delay to continue rotation such that geneva wheel can fully engage before measurement

// Hardware/Software Config
// Note: The limit switch must be on pin 2, and the change mode button on pin 3
const int SENSOR_PIN = 1;
const boolean VERBOSE = false;
const String HELP_MSG = "Valid commands are 'man', 'manual', 'auto', 'automatic', and numbers 1-4 while in manual mode.";

// ---- Motor Shield Setup
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *motor = AFMS.getMotor(1);

// ---- State Variables
int index = 1;
boolean manualMode = true;
unsigned long rotationTime = 0;
boolean stopFlag = false;

// ---- Running Code
void setup() {
  // Initilize Motor Shield
  AFMS.begin();
  motor->setSpeed(175);
  
  // Initialize serial communication at 9600 bits per second:
  Serial.begin(9600);   

  // Setup button and switch interrupts
  attachInterrupt(0, limitSwitchISR, RISING);
  
  // Calibrate
  calibrate();
}

/*
A calibration routine which will rotate the table to index 1,
and reset the time and index accordingly.
*/
void calibrate() {
  Serial.println("-- Calibrating, please wait --");
  
  // Start motor
  stopFlag = false;
  motor->run(FORWARD);
  
  // Wait until switch is triggered
  while(!stopFlag){
    delay(1);
  }
  
  // Wait for geneva wheel back to engage, then stop
  delay(MEASUREMENT_DELAY);
  stopFlag = false;
  motor->run(RELEASE);
  
  Serial.println(" -- Calibration Complete --"); 
  Serial.println("You are currently in manual mode.");
  Serial.println(HELP_MSG);
}

/*
Automatic mode is just the code below. Manual mode
is handled through serial.
Serial input is taken automatically after each loop.
*/
void loop() {
  if (!manualMode) {
    advanceOne();
    measure();
  }
}

/*
Measures the opacity of the petri dish
at the target index.
*/
void measureTarget (int target) {
  while (index != target) advanceOne();
  measure();
}

/*
Advances one quarter rotation, setting state variables and
recalibrating accordingly.
*/
void advanceOne() {
  unsigned long lastTime = millis();
  unsigned long currentTime;
  unsigned long stopTime = (index) * (QUARTER_ROTATION_PERIOD - MEASUREMENT_DELAY);
  
  if (VERBOSE) {
    Serial.println("------------");
    Serial.println("Beginning to advance one index.");
    Serial.println(" - Start index: " + String(index));
    Serial.println(" - Start Time: " + String(rotationTime));
    Serial.println(" - Scheduled Stop Time at " + String(stopTime));
    Serial.println("-----");
  }
  
  motor->run(FORWARD);
  
  // Run until the stop time condition is triggered,
  // or the stop flag is set to true by a switch interrupt
  while(rotationTime < stopTime && !stopFlag) {
    currentTime = millis();
    if (!stopFlag) rotationTime += currentTime - lastTime;
    lastTime = currentTime;
  }
    
  // Allow the back of the Geneva Wheel to fully engage before
  // stopping the motor.
  delay(MEASUREMENT_DELAY);
  motor->run(RELEASE);
  
  // Cycle the index
  index++;
  stopFlag = false;
  
  if (VERBOSE) {
    Serial.println("Advancement completed.");
    Serial.println(" - Current Time: " + String(rotationTime));
    Serial.println(" - New Index: " + String(index));
  }
  
  if (index == 5) {
    Serial.println(" Alignment error. ");
    calibrate();   
  }
}

/*
Read the voltage on the sensor pin when
called and output as an (index, normalized opacity)
tuple to the serial monitor.
*/
void measure() {
  int sensorValue = analogRead(SENSOR_PIN);
  float opacity = (sensorValue * (5.0 / 1023.0) - NORMALIZATION_CONST)/NORMALIZATION_FACTOR;
  Serial.print("(");
  Serial.print(index);
  Serial.print(", ");
  Serial.print(opacity);
  Serial.println(")");
}

// ---- Interrupt Service Routines and Events

/*
Called automatically between loops, and handles
input for manual mode or auto
*/
void serialEvent() {
  String inputString = "";

  while (Serial.available()) {
    char inChar = (char)Serial.read(); 
    if (inChar == '\n') break;
    else inputString += inChar;
  }

  // Interpret message and take appropriate action.
  if (inputString.equals("man") || inputString.equals("manual")) {
    manualMode = true;
    Serial.println("Manual mode enabled. Enter a number 1-4 to take a measurement of the relevant dish.");
  } else if (inputString.equals("auto") || inputString.equals("automatic")) {
    manualMode = false;
    Serial.println("Automatic mode enabled. Measurements will be taken and outputted in (dish-index, opacity) format.");
  } else if (inputString.toInt() <= 4 && inputString.toInt() > 0 && manualMode) {
    measureTarget(inputString.toInt());
  } else {
    Serial.println("Invalid input.");
    Serial.println(HELP_MSG);
  }
}

/*
When the limit switch is tripped, limitSwitchISR
is called, which sets the current index to 1, and
resets the measured time of rotation to 0.
*/
void limitSwitchISR() {
  static volatile unsigned long lastInterruptTime = 0;
  static volatile unsigned long time;
  time = millis();
  if (time - lastInterruptTime > SWITCH_DEBOUNCE) {
    if (VERBOSE) Serial.println("Limit switch tripped.");
    rotationTime = 0;
    index = 0;
    stopFlag = true;
    lastInterruptTime = time;
  }
}
