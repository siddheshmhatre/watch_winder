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

// Motor state for non-blocking operation
enum MotorState {
    MOTOR_IDLE,
    MOTOR_RUNNING
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

    // Non-blocking state
    MotorState state;
    bool currentDirection;
    unsigned long targetEndTime;
    unsigned long lastStepTime;
    int totalSteps;

public:
    Stepper(int in1, int in2, int in3, int in4) {
        pins[0] = in1;
        pins[1] = in2;
        pins[2] = in3;
        pins[3] = in4;
        currentStep = 0;
        lastDirectionCW = true;
        stepDelay = STEP_DELAY_MS;
        state = MOTOR_IDLE;
        totalSteps = 0;
        lastStepTime = 0;
        targetEndTime = 0;
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
            currentStep--;
            if (currentStep < 0) currentStep = 7;
        } else {
            currentStep++;
            if (currentStep >= 8) currentStep = 0;
        }

        for (int i = 0; i < 4; i++) {
            digitalWrite(pins[i], STEP_SEQUENCE[currentStep][i]);
        }
    }

    void stop() {
        state = MOTOR_IDLE;
        // De-energize all coils to save power and reduce heat
        for (int i = 0; i < 4; i++) {
            digitalWrite(pins[i], LOW);
        }
    }

    // Start a non-blocking rotation for a duration
    void startRotation(int seconds, Direction dir) {
        if (dir == DIR_BIDIRECTIONAL) {
            currentDirection = !lastDirectionCW;
            lastDirectionCW = currentDirection;
        } else {
            currentDirection = (dir == DIR_CLOCKWISE);
        }

        targetEndTime = millis() + (unsigned long)seconds * 1000;
        lastStepTime = millis();
        totalSteps = 0;
        state = MOTOR_RUNNING;
    }

    // Call this from the main loop - non-blocking
    // Returns true if motor is still running
    bool update() {
        if (state != MOTOR_RUNNING) {
            return false;
        }

        unsigned long now = millis();

        // Check if rotation time is complete
        if (now >= targetEndTime) {
            stop();
            return false;
        }

        // Check if it's time for the next step
        if (now - lastStepTime >= stepDelay) {
            stepMotor(currentDirection);
            totalSteps++;
            lastStepTime = now;
        }

        return true;
    }

    // Check if motor is currently running
    bool isRunning() {
        return state == MOTOR_RUNNING;
    }

    // Get turns completed in current/last rotation
    float getTurnsCompleted() {
        return (float)totalSteps / (float)HALF_STEPS_PER_REVOLUTION;
    }

    bool getLastDirection() {
        return lastDirectionCW;
    }

    // Legacy blocking function - only use for testing if needed
    float rotateForDurationBlocking(int seconds, Direction dir) {
        startRotation(seconds, dir);
        while (update()) {
            yield();
        }
        return getTurnsCompleted();
    }
};

#endif // STEPPER_H
