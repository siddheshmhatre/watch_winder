#ifndef STEPPER_H
#define STEPPER_H

#include <Arduino.h>
#include "config.h"

// Direction enumeration
enum Direction {
    DIR_CLOCKWISE = 0,
    DIR_COUNTER_CLOCKWISE = 1,
    DIR_BIDIRECTIONAL = 2
};

// Half-step sequence for 28BYJ-48 (smoother operation)
const int STEP_SEQUENCE[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

class Stepper {
private:
    int pins[4];
    int currentStep;
    bool lastDirectionCW;  // For bidirectional mode
    unsigned long stepDelay;

public:
    Stepper(int in1, int in2, int in3, int in4) {
        pins[0] = in1;
        pins[1] = in2;
        pins[2] = in3;
        pins[3] = in4;
        currentStep = 0;
        lastDirectionCW = true;
        stepDelay = STEP_DELAY_MS;
    }

    void begin() {
        for (int i = 0; i < 4; i++) {
            pinMode(pins[i], OUTPUT);
            digitalWrite(pins[i], LOW);
        }
    }

    void setSpeed(unsigned long delayMs) {
        stepDelay = delayMs;
    }

    void stepMotor(bool clockwise) {
        if (clockwise) {
            currentStep++;
            if (currentStep >= 8) currentStep = 0;
        } else {
            currentStep--;
            if (currentStep < 0) currentStep = 7;
        }

        for (int i = 0; i < 4; i++) {
            digitalWrite(pins[i], STEP_SEQUENCE[currentStep][i]);
        }
    }

    void stop() {
        // De-energize all coils to save power and reduce heat
        for (int i = 0; i < 4; i++) {
            digitalWrite(pins[i], LOW);
        }
    }

    // Rotate a specific number of steps
    void rotate(int steps, Direction dir) {
        bool clockwise;

        if (dir == DIR_BIDIRECTIONAL) {
            clockwise = !lastDirectionCW;
            lastDirectionCW = clockwise;
        } else {
            clockwise = (dir == DIR_CLOCKWISE);
        }

        for (int i = 0; i < steps; i++) {
            stepMotor(clockwise);
            delay(stepDelay);
            yield();  // Allow ESP8266 background tasks
        }

        stop();
    }

    // Rotate for a specific number of full turns
    void rotateTurns(float turns, Direction dir) {
        // Half-stepping = 4096 steps per revolution
        int steps = (int)(turns * 4096);
        rotate(steps, dir);
    }

    // Rotate for a specific duration (seconds)
    // Returns approximate number of turns completed
    float rotateForDuration(int seconds, Direction dir) {
        bool clockwise;

        if (dir == DIR_BIDIRECTIONAL) {
            clockwise = !lastDirectionCW;
            lastDirectionCW = clockwise;
        } else {
            clockwise = (dir == DIR_CLOCKWISE);
        }

        unsigned long startTime = millis();
        unsigned long duration = (unsigned long)seconds * 1000;
        int totalSteps = 0;

        while (millis() - startTime < duration) {
            stepMotor(clockwise);
            totalSteps++;
            delay(stepDelay);
            yield();  // Allow ESP8266 background tasks
        }

        stop();

        // Return turns completed (4096 half-steps per revolution)
        return (float)totalSteps / 4096.0f;
    }

    bool getLastDirection() {
        return lastDirectionCW;
    }
};

#endif // STEPPER_H
