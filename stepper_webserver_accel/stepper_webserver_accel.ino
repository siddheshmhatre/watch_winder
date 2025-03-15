#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AccelStepper.h>

const char* ssid = "ESP8266_Stepper_Control";
const char* password = "watchmeroll";

ESP8266WebServer server(80);

#define FULLSTEP 4
#define STEPS_PER_REVOLUTION 2048

AccelStepper stepper(FULLSTEP, D1, D5, D2, D6);

// Winder Settings
struct WinderSettings {
    int tpd;                    // Turns Per Day
    bool bidirectional;         // true = bidirectional, false = clockwise
    float hoursPerDay;           // Operating hours per day
    int rotationTimeSeconds;    // Time to complete rotations
    float restTimeMinutes;       // Rest time between rotations
} settings;

// Operating variables
volatile bool isRunning = false;
volatile bool currentDirection = true;  // true = CW, false = CCW
volatile int completedTurns = 0;
volatile long stepCounter = 0;
unsigned long lastRotationTime = 0;
unsigned long restStartTime = 0;
bool isResting = false;
int calculatedSpeed = 0;
unsigned long winderStartTime = 0;      // When winder was first started
unsigned long dailyCycleStartTime = 0;  // Start of current 24hr cycle
bool dailyLimitReached = false;         // Whether daily hour limit is reached

// Add debug function
void debugLog(String message) {
    //unsigned long timeStamp = millis()/1000; // seconds since start
    //Serial.print(timeStamp);
    //Serial.print("s: ");
    Serial.println(message);
}

void setup() {
    Serial.begin(115200);
    
    stepper.setMaxSpeed(1000.0);
    stepper.setAcceleration(50.0);
    stepper.setCurrentPosition(0);

    WiFi.softAP(ssid, password);
    
    server.on("/", handleRoot);
    server.on("/settings", HTTP_POST, handleSettings);
    server.on("/control", HTTP_POST, handleControl);
    server.on("/status", HTTP_GET, handleStatus);
    server.begin();
}

void calculateSpeed() {
    // Calculate turns needed during active hours
    float turnsPerHour = (float)settings.tpd / settings.hoursPerDay;

    float numPeriodsPerHour = 3600.0 / ((float)settings.rotationTimeSeconds + (float)settings.restTimeMinutes * 60);
    
    float turnsPerSecond = turnsPerHour / (numPeriodsPerHour * (float)settings.rotationTimeSeconds);

    calculatedSpeed = (int)(turnsPerSecond * STEPS_PER_REVOLUTION);

    // Cap speed at 500 steps/second
    const int MAX_SPEED = 450;
    if (calculatedSpeed > MAX_SPEED) {
        debugLog("WARNING: Speed " + String(calculatedSpeed) + " exceeds maximum!");
        debugLog("Limiting to " + String(MAX_SPEED) + " steps/sec");
        calculatedSpeed = MAX_SPEED;
    }

    debugLog("Speed calculation:");
    debugLog("TPD: " + String(settings.tpd));
    debugLog("Turns/hour: " + String(turnsPerHour));
    debugLog("Periods/hour: " + String(numPeriodsPerHour));
    debugLog("Calculated speed: " + String(calculatedSpeed) + " steps/sec");
}

void loop() {
    server.handleClient();
    
    // Initialize start times if just started
    if (isRunning && winderStartTime == 0) {
        winderStartTime = millis();
        dailyCycleStartTime = winderStartTime;
        dailyLimitReached = false;
        debugLog("Winder operation started. Will run for " + String(settings.hoursPerDay) + " hours per day.");
    }
    
    // Check if we need to reset the daily cycle (every 24 hours)
    if (isRunning && (millis() - dailyCycleStartTime > 24 * 60 * 60 * 1000UL)) {
        dailyCycleStartTime = millis();
        dailyLimitReached = false;
        debugLog("New 24-hour cycle started");
    }
    
    // Calculate total operating time (including rest periods)
    if (isRunning && !dailyLimitReached) {
        float operatingTimeToday = (float)(millis() - dailyCycleStartTime) / 3600000; // convert to hours
        debugLog("Operating time today: " + String(operatingTimeToday) + " hours");
        
        // Check if daily limit reached
        if (operatingTimeToday >= settings.hoursPerDay) {
            dailyLimitReached = true;
            stepper.stop();
            debugLog("Daily operation limit of " + String(settings.hoursPerDay) + 
                   " hours reached. Will resume tomorrow.");
        }
    }
    
    // Only run motor if within daily limit
    if (isRunning && !dailyLimitReached) {
        // Handle rotation/rest logic
        if (!isResting) {
            unsigned long currentTime = millis();
            
            // Check rotation time limit
            if (currentTime - lastRotationTime >= settings.rotationTimeSeconds * 1000) {
                isResting = true;
                restStartTime = currentTime;
                stepper.stop();
                debugLog("Entering rest period");
            } else {
                // Continue rotation
                stepper.setSpeed(currentDirection ? -calculatedSpeed : calculatedSpeed);
                if (stepper.runSpeed()) {
                    stepCounter++;
                    
                    // Check for full rotation completion
                    if (stepCounter >= STEPS_PER_REVOLUTION) {
                        stepCounter = 0;
                        if (settings.bidirectional) {
                            currentDirection = !currentDirection;
                            debugLog("Full rotation complete. Direction changed to: " + 
                                   String(currentDirection ? "CW" : "CCW"));
                        }
                    }
                }
            }
        } else {
            // Rest period logic
            if (millis() - restStartTime >= (unsigned long) (settings.restTimeMinutes * 60000)) {
                isResting = false;
                lastRotationTime = millis();
                debugLog("Rest period complete, resuming rotation");
            }
        }
    }
    
    // Reset timing variables if stopped
    if (!isRunning) {
        winderStartTime = 0;
        dailyCycleStartTime = 0;
        dailyLimitReached = false;
    }

}

void handleSettings() {
    if (server.hasArg("tpd")) {
        // Get previous direction setting
        bool wasBidirectional = settings.bidirectional;
        
        // Update settings
        settings.tpd = server.arg("tpd").toInt();
        settings.bidirectional = server.arg("direction") == "bidirectional";
        settings.hoursPerDay = server.arg("hours").toFloat();
        settings.rotationTimeSeconds = server.arg("rotationTime").toInt();
        settings.restTimeMinutes = server.arg("restTime").toFloat();
        
        // Reset direction when changing modes
        if (wasBidirectional != settings.bidirectional) {
            // If changed to clockwise-only, force the correct direction
            if (!settings.bidirectional) {
                currentDirection = true;  // Reset to true (CW)
                debugLog("Switching to clockwise-only mode");
            }
        }
        
        // Log settings
        debugLog("\nNew settings received:");
        debugLog("TPD: " + String(settings.tpd));
        debugLog("Direction: " + String(settings.bidirectional ? "Bidirectional" : "Clockwise"));
        debugLog("Hours/day: " + String(settings.hoursPerDay, 2)); // 2 decimal places
        debugLog("Rotation time: " + String(settings.rotationTimeSeconds) + "s");
        debugLog("Rest time: " + String(settings.restTimeMinutes, 2) + "m");
        
        calculateSpeed();
        server.send(200, "text/plain", "Settings updated");
    }
}

void handleControl() {
    if (server.hasArg("action")) {
        String action = server.arg("action");
        if (action == "start") {
            isRunning = true;
            lastRotationTime = millis();
            isResting = false;
            winderStartTime = millis();
            dailyCycleStartTime = millis();
            dailyLimitReached = false;
            
            // Always reset direction to clockwise when starting
            if (!settings.bidirectional) {
                currentDirection = true; // Force clockwise for non-bidirectional mode
            }
            
            debugLog("\nStarting winder with following settings:");
            debugLog("TPD: " + String(settings.tpd));
            debugLog("Direction: " + String(settings.bidirectional ? "Bidirectional" : "Clockwise"));
            debugLog("Hours/day: " + String(settings.hoursPerDay));
            debugLog("Rotation time: " + String(settings.rotationTimeSeconds) + "s");
            debugLog("Rest time: " + String(settings.restTimeMinutes) + "m");
            debugLog("Calculated speed: " + String(calculatedSpeed) + " steps/sec");
            debugLog("Starting direction: " + String(currentDirection ? "CW" : "CCW"));

        } else if (action == "stop") {
            isRunning = false;
            stepper.stop();
            debugLog("Stopping winder");
        }
        server.send(200, "text/plain", "OK");
    }
}

// Add new endpoint handler after handleControl()
void handleStatus() {
    String status = String(calculatedSpeed);
    String nextAction = "";
    
    if (isRunning) {
        if (dailyLimitReached) {
            // Calculate time until next day cycle starts
            unsigned long timeNow = millis();
            unsigned long cycleEndTime = dailyCycleStartTime + (24 * 60 * 60 * 1000UL);
            unsigned long remainingTime = cycleEndTime - timeNow;
            
            int hoursLeft = (remainingTime / 3600000);
            int minutesLeft = (remainingTime % 3600000) / 60000;
            
            nextAction = "Daily limit reached. Next cycle in " + 
                         String(hoursLeft) + "h " + String(minutesLeft) + "m";
        }
        else if (isResting) {
            // Calculate remaining rest time
            unsigned long timeNow = millis();
            unsigned long restEndTime = restStartTime + (unsigned long)(settings.restTimeMinutes * 60000);
            unsigned long remainingTime = (timeNow < restEndTime) ? (restEndTime - timeNow) : 0;
            
            int secondsLeft = remainingTime / 1000;
            nextAction = "Resting. Will rotate in " + String(secondsLeft) + " seconds";
        }
        else {
            // Calculate remaining rotation time
            unsigned long timeNow = millis();
            unsigned long rotationEndTime = lastRotationTime + (settings.rotationTimeSeconds * 1000);
            unsigned long remainingTime = (timeNow < rotationEndTime) ? (rotationEndTime - timeNow) : 0;
            
            int secondsLeft = remainingTime / 1000;
            nextAction = "Rotating for " + String(secondsLeft) + " more seconds";
        }
    }
    else {
        nextAction = "Stopped";
    }
    
    // Return both speed and next action
    status += "|" + nextAction;
    server.send(200, "text/plain", status);
}


void handleRoot() {
    String html = R"====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: 'Roboto', sans-serif;
            margin: 0;
            padding: 20px;
            background: #f5f5f5;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #333;
            font-weight: bold;
        }
        input[type="number"], select {
            width: 100%;
            padding: 10px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 16px;
        }
        button {
            background: #2196F3;
            color: white;
            border: none;
            padding: 12px 20px;
            border-radius: 5px;
            font-size: 16px;
            cursor: pointer;
            width: 100%;
            margin-top: 10px;
        }
        button:active {
            background: #1976D2;
        }
        #startStop {
            background: #4CAF50;
            margin-top: 20px;
        }
        #startStop.stop {
            background: #f44336;
        }
        .header {
            text-align: center;
            margin-bottom: 20px;
        }
        h1 {
            color: #333;
            margin: 0;
            padding: 10px 0;
        }
        .status-display {
        padding: 10px;
        background: #f0f0f0;
        border-radius: 5px;
        font-size: 16px;
        font-weight: bold;
        text-align: center;
        }
        .status-bar {
               margin-top: 20px;
               padding: 10px;
               background: #333;
               color: white;
               border-radius: 5px;
               text-align: center;
               font-weight: bold;
           }
           
           .status-display {
               padding: 10px;
               background: #f0f0f0;
               border-radius: 5px;
               font-size: 16px;
               font-weight: bold;
               text-align: center;
               margin-bottom: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Watch Winder Control</h1>
        </div>
        <div class="status-bar" id="statusBar">Status: Loading...</div>
        </div>
        <div class="form-group">
            <label for="tpd">Turns Per Day (TPD):</label>
            <input type="number" id="tpd" value="650" min="500" max="1200">
        </div>
        <div class="form-group">
            <label for="direction">Rotation Direction:</label>
            <select id="direction">
                <option value="clockwise">Clockwise</option>
                <option value="bidirectional">Bidirectional</option>
            </select>
        </div>
        <div class="form-group">
            <label for="hours">Hours Per Day:</label>
            <input type="number" id="hours" value="4" min="1" max="12">
        </div>
        <div class="form-group">
            <label for="rotationTime">Rotation Time (seconds):</label>
            <input type="number" id="rotationTime" value="30" min="10" max="120">
        </div>
        <div class="form-group">
            <label for="restTime">Rest Time (minutes):</label>
            <input type="number" id="restTime" value="3" min="1" max="120">
        </div>
        <div class="form-group">
        <label>Current Speed (steps/sec):</label>
        <div id="speedDisplay" class="status-display">--</div>
        </div>
        <button onclick="updateSettings()">Update Settings</button>
        <button id="startStop" onclick="toggleStart()">Start</button>
    </div>
    <script>
        let isRunning = false;
        
        function updateSettings() {
            const settings = {
                tpd: document.getElementById('tpd').value,
                direction: document.getElementById('direction').value,
                hours: document.getElementById('hours').value,
                rotationTime: document.getElementById('rotationTime').value,
                restTime: document.getElementById('restTime').value
            };
            
            fetch('/settings', {
                method: 'POST',
                body: new URLSearchParams(settings)
            });
            fetch('/settings', {
                method: 'POST',
                body: new URLSearchParams(settings)
            })
            .then(response => response.text())
            .then(() => {
                updateStatus();  // Update speed display after settings are saved
            });
        }
        
        function toggleStart() {
            isRunning = !isRunning;
            const btn = document.getElementById('startStop');
            btn.textContent = isRunning ? 'Stop' : 'Start';
            btn.classList.toggle('stop');
            
            fetch('/control', {
                method: 'POST',
                body: new URLSearchParams({
                    action: isRunning ? 'start' : 'stop'
                })
            });
        }
        function updateStatus() {
               fetch('/status')
                   .then(response => response.text())
                   .then(data => {
                       const parts = data.split('|');
                       if (parts.length >= 2) {
                           document.getElementById('speedDisplay').textContent = parts[0];
                           document.getElementById('statusBar').textContent = "Status: " + parts[1];
                       }
                   });
           }
           
        // Update status more frequently (every second)
        setInterval(updateStatus, 1000);
        
        // Initial status update
        updateStatus();
    
    </script>
</body>
</html>
)====";
    server.send(200, "text/html", html);
}