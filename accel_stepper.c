/*
 * This ESP8266 NodeMCU code was developed by newbiely.com
 *
 * This ESP8266 NodeMCU code is made available for public use without any restriction
 *
 * For comprehensive instructions and wiring diagrams, please visit:
 * https://newbiely.com/tutorials/esp8266/esp8266-28byj-48-stepper-motor-uln2003-driver
 */

// Include the AccelStepper Library
#include <AccelStepper.h>

// define step constant
#define FULLSTEP 4
#define STEP_PER_REVOLUTION 2048 // this value is from datasheet

// The ESP8266 pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
AccelStepper stepper(FULLSTEP, D1, D5, D2, D6);

void setup() {
  Serial.begin(9600);
  stepper.setMaxSpeed(1000.0);   // set the maximum speed
  stepper.setAcceleration(50.0); // set acceleration
  stepper.setSpeed(200);         // set initial speed
  stepper.setCurrentPosition(0); // set position
  stepper.moveTo(STEP_PER_REVOLUTION); // set target position: 64 steps <=> one revolution
}

void loop() {
  // change direction once the motor reaches target position
  if (stepper.distanceToGo() == 0)
    stepper.moveTo(-stepper.currentPosition());

  stepper.run(); // MUST be called in loop() function

  Serial.print(F("Current Position: "));
  Serial.println(stepper.currentPosition());
}
