#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>
#include "config.h"
#include "stepper.h"

// Motor settings structure
struct MotorSettings {
    bool enabled;
    Direction direction;
    int turnsPerDay;       // TPD
    int activeHours;       // Hours of operation per day
    int rotationTime;      // Seconds per rotation burst
    int restTime;          // Minutes between rotations

    // Calculated values
    float turnsPerCycle;
    int cyclesPerDay;
    unsigned long cycleDurationMs;
};

class Scheduler {
private:
    Stepper* motor;
    MotorSettings settings;
    unsigned long lastCycleTime;
    int completedCycles;
    float totalTurnsToday;
    bool isRunning;
    int motorId;

public:
    Scheduler(Stepper* stepper, int id) {
        motor = stepper;
        motorId = id;
        lastCycleTime = 0;
        completedCycles = 0;
        totalTurnsToday = 0;
        isRunning = false;

        // Initialize with defaults
        settings.enabled = true;
        settings.direction = (Direction)DEFAULT_DIRECTION;
        settings.turnsPerDay = DEFAULT_TPD;
        settings.activeHours = DEFAULT_ACTIVE_HOURS;
        settings.rotationTime = DEFAULT_ROTATION_TIME;
        settings.restTime = DEFAULT_REST_TIME;

        calculateSchedule();
    }

    void calculateSchedule() {
        // Calculate cycle duration in milliseconds
        // Cycle = rotation time + rest time
        settings.cycleDurationMs = (unsigned long)(settings.rotationTime * 1000) +
                                   (unsigned long)(settings.restTime * 60 * 1000);

        // Calculate cycles per day based on active hours
        unsigned long activeMs = (unsigned long)settings.activeHours * 60 * 60 * 1000;
        settings.cyclesPerDay = activeMs / settings.cycleDurationMs;

        // Ensure at least 1 cycle
        if (settings.cyclesPerDay < 1) {
            settings.cyclesPerDay = 1;
        }

        // Calculate turns needed per cycle to achieve TPD
        settings.turnsPerCycle = (float)settings.turnsPerDay / (float)settings.cyclesPerDay;
    }

    void setSettings(bool enabled, int direction, int tpd, int activeHours,
                     int rotationTime, int restTime) {
        settings.enabled = enabled;
        settings.direction = (Direction)direction;
        settings.turnsPerDay = tpd;
        settings.activeHours = activeHours;
        settings.rotationTime = rotationTime;
        settings.restTime = restTime;

        calculateSchedule();

        // Reset daily counters when settings change
        completedCycles = 0;
        totalTurnsToday = 0;
    }

    MotorSettings getSettings() {
        return settings;
    }

    void start() {
        isRunning = true;
        lastCycleTime = millis() - settings.cycleDurationMs;  // Trigger immediate first cycle
    }

    void stop() {
        isRunning = false;
        motor->stop();
    }

    bool getRunning() {
        return isRunning;
    }

    int getCompletedCycles() {
        return completedCycles;
    }

    float getTotalTurns() {
        return totalTurnsToday;
    }

    void resetDailyCounters() {
        completedCycles = 0;
        totalTurnsToday = 0;
    }

    // Call this in the main loop - non-blocking
    bool update() {
        if (!isRunning || !settings.enabled) {
            return false;
        }

        unsigned long currentTime = millis();

        // Check if enough time has passed for next cycle
        if (currentTime - lastCycleTime >= settings.cycleDurationMs) {
            // Check if we've reached daily target
            if (completedCycles >= settings.cyclesPerDay) {
                return false;  // Done for today
            }

            // Execute rotation
            lastCycleTime = currentTime;

            // Rotate for the configured duration
            float turnsCompleted = motor->rotateForDuration(settings.rotationTime,
                                                            settings.direction);

            totalTurnsToday += turnsCompleted;
            completedCycles++;

            Serial.printf("Motor %d: Cycle %d/%d, Turns this cycle: %.2f, Total: %.2f\n",
                         motorId, completedCycles, settings.cyclesPerDay,
                         turnsCompleted, totalTurnsToday);

            return true;  // Cycle was executed
        }

        return false;  // No action taken
    }

    // Get status as JSON-compatible values
    void getStatus(bool& running, int& cycles, int& totalCycles,
                   float& turns, int& targetTpd) {
        running = isRunning;
        cycles = completedCycles;
        totalCycles = settings.cyclesPerDay;
        turns = totalTurnsToday;
        targetTpd = settings.turnsPerDay;
    }

    // Get time until next cycle in seconds
    unsigned long getTimeUntilNextCycle() {
        if (!isRunning || !settings.enabled) {
            return 0;
        }

        unsigned long elapsed = millis() - lastCycleTime;
        if (elapsed >= settings.cycleDurationMs) {
            return 0;
        }

        return (settings.cycleDurationMs - elapsed) / 1000;
    }
};

#endif // SCHEDULER_H
