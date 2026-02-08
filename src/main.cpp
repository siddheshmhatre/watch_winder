#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "stepper.h"
#include "scheduler.h"

// Global objects
ESP8266WebServer server(WEB_SERVER_PORT);
DNSServer dnsServer;

Stepper motor1(MOTOR1_IN1, MOTOR1_IN2, MOTOR1_IN3, MOTOR1_IN4);
Stepper motor2(MOTOR2_IN1, MOTOR2_IN2, MOTOR2_IN3, MOTOR2_IN4);

Scheduler scheduler1(&motor1, 1);
Scheduler scheduler2(&motor2, 2);

bool apMode = false;
String storedSSID = "";
String storedPassword = "";

// Forward declarations
void setupWiFi();
void setupWebServer();
void loadSettings();
void saveSettings();
void handleRoot();
void handleGetStatus();
void handleGetSettings();
void handleSetSettings();
void handleStart();
void handleStop();
void handleTestMotor();
void handleWiFiScan();
void handleWiFiConnect();
void handleNotFound();

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println("\n\n=================================");
    Serial.println("  Watch Winder Controller v1.0");
    Serial.println("=================================\n");

    // Initialize file system
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS, formatting...");
        LittleFS.format();
        LittleFS.begin();
    }

    // Initialize motors
    motor1.begin();
    motor2.begin();
    Serial.println("Motors initialized");

    // Load saved settings
    loadSettings();

    // Setup WiFi
    setupWiFi();

    // Setup web server
    setupWebServer();

    Serial.println("\nSystem ready!");
}

void loop() {
    // Handle DNS for captive portal
    if (apMode) {
        dnsServer.processNextRequest();
    } else {
        MDNS.update();
    }

    // Handle web requests
    server.handleClient();

    // Update motors directly (for test mode)
    motor1.update();
    motor2.update();

    // Update schedulers (non-blocking)
    scheduler1.update();
    scheduler2.update();

    yield();
}

void setupWiFi() {
    // Check if we have stored credentials
    if (storedSSID.length() > 0) {
        Serial.printf("Connecting to WiFi: %s\n", storedSSID.c_str());

        WiFi.mode(WIFI_STA);
        WiFi.begin(storedSSID.c_str(), storedPassword.c_str());

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

            // Start mDNS responder
            if (MDNS.begin("watchwinder")) {
                Serial.println("mDNS started: http://watchwinder.local");
                MDNS.addService("http", "tcp", 80);
            }

            apMode = false;
            return;
        }

        Serial.println("\nWiFi connection failed");
    }

    // Start Access Point for setup
    Serial.println("Starting Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    Serial.printf("AP started: %s\n", AP_SSID);
    Serial.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    apMode = true;
}

void setupWebServer() {
    // API endpoints - register these FIRST
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings", HTTP_POST, handleSetSettings);
    server.on("/api/start", HTTP_POST, handleStart);
    server.on("/api/stop", HTTP_POST, handleStop);
    server.on("/api/test", HTTP_POST, handleTestMotor);
    server.on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
    server.on("/api/wifi/connect", HTTP_POST, handleWiFiConnect);

    // Serve static files explicitly
    server.on("/style.css", HTTP_GET, []() {
        File file = LittleFS.open("/style.css", "r");
        if (file) {
            server.streamFile(file, "text/css");
            file.close();
        } else {
            server.send(404, "text/plain", "Not found");
        }
    });

    server.on("/app.js", HTTP_GET, []() {
        File file = LittleFS.open("/app.js", "r");
        if (file) {
            server.streamFile(file, "application/javascript");
            file.close();
        } else {
            server.send(404, "text/plain", "Not found");
        }
    });

    // Captive portal - redirect all requests to root
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("Web server started");
}

void handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
        server.streamFile(file, "text/html");
        file.close();
    } else {
        server.send(200, "text/html",
            "<!DOCTYPE html><html><head><title>Watch Winder</title></head>"
            "<body><h1>Watch Winder</h1>"
            "<p>Web interface files not found. Please upload the data folder.</p>"
            "<p>Run: <code>pio run --target uploadfs</code></p></body></html>");
    }
}

void handleGetStatus() {
    StaticJsonDocument<512> doc;

    doc["apMode"] = apMode;
    doc["ip"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    doc["uptime"] = millis() / 1000;

    // Motor 1 status
    JsonObject m1 = doc.createNestedObject("motor1");
    bool running1;
    int cycles1, totalCycles1, targetTpd1;
    float turns1;
    scheduler1.getStatus(running1, cycles1, totalCycles1, turns1, targetTpd1);
    m1["running"] = running1;
    m1["cycles"] = cycles1;
    m1["totalCycles"] = totalCycles1;
    m1["turns"] = turns1;
    m1["targetTpd"] = targetTpd1;
    m1["nextCycle"] = scheduler1.getTimeUntilNextCycle();

    // Motor 2 status
    JsonObject m2 = doc.createNestedObject("motor2");
    bool running2;
    int cycles2, totalCycles2, targetTpd2;
    float turns2;
    scheduler2.getStatus(running2, cycles2, totalCycles2, turns2, targetTpd2);
    m2["running"] = running2;
    m2["cycles"] = cycles2;
    m2["totalCycles"] = totalCycles2;
    m2["turns"] = turns2;
    m2["targetTpd"] = targetTpd2;
    m2["nextCycle"] = scheduler2.getTimeUntilNextCycle();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetSettings() {
    StaticJsonDocument<512> doc;

    MotorSettings s1 = scheduler1.getSettings();
    JsonObject m1 = doc.createNestedObject("motor1");
    m1["enabled"] = s1.enabled;
    m1["direction"] = s1.direction;
    m1["tpd"] = s1.turnsPerDay;
    m1["activeHours"] = s1.activeHours;
    m1["rotationTime"] = s1.rotationTime;
    m1["restTime"] = s1.restTime;
    m1["cyclesPerDay"] = s1.cyclesPerDay;
    m1["turnsPerCycle"] = s1.turnsPerCycle;

    MotorSettings s2 = scheduler2.getSettings();
    JsonObject m2 = doc.createNestedObject("motor2");
    m2["enabled"] = s2.enabled;
    m2["direction"] = s2.direction;
    m2["tpd"] = s2.turnsPerDay;
    m2["activeHours"] = s2.activeHours;
    m2["rotationTime"] = s2.rotationTime;
    m2["restTime"] = s2.restTime;
    m2["cyclesPerDay"] = s2.cyclesPerDay;
    m2["turnsPerCycle"] = s2.turnsPerCycle;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetSettings() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (doc.containsKey("motor1")) {
        JsonObject m1 = doc["motor1"];
        scheduler1.setSettings(
            m1["enabled"] | true,
            m1["direction"] | 0,
            m1["tpd"] | DEFAULT_TPD,
            m1["activeHours"] | DEFAULT_ACTIVE_HOURS,
            m1["rotationTime"] | DEFAULT_ROTATION_TIME,
            m1["restTime"] | DEFAULT_REST_TIME
        );
    }

    if (doc.containsKey("motor2")) {
        JsonObject m2 = doc["motor2"];
        scheduler2.setSettings(
            m2["enabled"] | true,
            m2["direction"] | 0,
            m2["tpd"] | DEFAULT_TPD,
            m2["activeHours"] | DEFAULT_ACTIVE_HOURS,
            m2["rotationTime"] | DEFAULT_ROTATION_TIME,
            m2["restTime"] | DEFAULT_REST_TIME
        );
    }

    saveSettings();
    server.send(200, "application/json", "{\"success\":true}");
}

void handleStart() {
    StaticJsonDocument<64> doc;
    if (server.hasArg("plain")) {
        deserializeJson(doc, server.arg("plain"));
    }

    int motor = doc["motor"] | 0;  // 0 = both, 1 = motor1, 2 = motor2

    if (motor == 0 || motor == 1) {
        scheduler1.start();
        Serial.println("Motor 1 started");
    }
    if (motor == 0 || motor == 2) {
        scheduler2.start();
        Serial.println("Motor 2 started");
    }

    server.send(200, "application/json", "{\"success\":true}");
}

void handleStop() {
    StaticJsonDocument<64> doc;
    if (server.hasArg("plain")) {
        deserializeJson(doc, server.arg("plain"));
    }

    int motor = doc["motor"] | 0;  // 0 = both, 1 = motor1, 2 = motor2

    if (motor == 0 || motor == 1) {
        scheduler1.stop();
        Serial.println("Motor 1 stopped");
    }
    if (motor == 0 || motor == 2) {
        scheduler2.stop();
        Serial.println("Motor 2 stopped");
    }

    server.send(200, "application/json", "{\"success\":true}");
}

void handleTestMotor() {
    StaticJsonDocument<64> doc;
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    deserializeJson(doc, server.arg("plain"));

    int motor = doc["motor"] | 1;
    int direction = doc["direction"] | 0;
    int duration = doc["duration"] | 3;  // seconds

    Serial.printf("Testing motor %d, direction %d, duration %d sec\n",
                  motor, direction, duration);

    // Start motor rotation (non-blocking) - motor will run in main loop
    if (motor == 1) {
        motor1.startRotation(duration, (Direction)direction);
    } else if (motor == 2) {
        motor2.startRotation(duration, (Direction)direction);
    }

    // Send response immediately - motor runs in background
    server.send(200, "application/json", "{\"success\":true}");
}

void handleWiFiScan() {
    int n = WiFi.scanNetworks();

    StaticJsonDocument<1024> doc;
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < n && i < 10; i++) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["secure"] = WiFi.encryptionType(i) != ENC_TYPE_NONE;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleWiFiConnect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    storedSSID = doc["ssid"].as<String>();
    storedPassword = doc["password"].as<String>();

    saveSettings();

    server.send(200, "application/json",
        "{\"success\":true,\"message\":\"Credentials saved. Rebooting...\"}");

    delay(1000);
    ESP.restart();
}

void handleNotFound() {
    // Captive portal redirect
    if (apMode) {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "Not found");
    }
}

void loadSettings() {
    File file = LittleFS.open(SETTINGS_FILE, "r");
    if (!file) {
        Serial.println("No settings file found, using defaults");
        return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.println("Failed to parse settings file");
        return;
    }

    // Load WiFi credentials
    storedSSID = doc["wifi"]["ssid"].as<String>();
    storedPassword = doc["wifi"]["password"].as<String>();

    // Load motor 1 settings
    if (doc.containsKey("motor1")) {
        JsonObject m1 = doc["motor1"];
        scheduler1.setSettings(
            m1["enabled"] | true,
            m1["direction"] | 0,
            m1["tpd"] | DEFAULT_TPD,
            m1["activeHours"] | DEFAULT_ACTIVE_HOURS,
            m1["rotationTime"] | DEFAULT_ROTATION_TIME,
            m1["restTime"] | DEFAULT_REST_TIME
        );
    }

    // Load motor 2 settings
    if (doc.containsKey("motor2")) {
        JsonObject m2 = doc["motor2"];
        scheduler2.setSettings(
            m2["enabled"] | true,
            m2["direction"] | 0,
            m2["tpd"] | DEFAULT_TPD,
            m2["activeHours"] | DEFAULT_ACTIVE_HOURS,
            m2["rotationTime"] | DEFAULT_ROTATION_TIME,
            m2["restTime"] | DEFAULT_REST_TIME
        );
    }

    Serial.println("Settings loaded");
}

void saveSettings() {
    StaticJsonDocument<1024> doc;

    // Save WiFi credentials
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = storedSSID;
    wifi["password"] = storedPassword;

    // Save motor 1 settings
    MotorSettings s1 = scheduler1.getSettings();
    JsonObject m1 = doc.createNestedObject("motor1");
    m1["enabled"] = s1.enabled;
    m1["direction"] = s1.direction;
    m1["tpd"] = s1.turnsPerDay;
    m1["activeHours"] = s1.activeHours;
    m1["rotationTime"] = s1.rotationTime;
    m1["restTime"] = s1.restTime;

    // Save motor 2 settings
    MotorSettings s2 = scheduler2.getSettings();
    JsonObject m2 = doc.createNestedObject("motor2");
    m2["enabled"] = s2.enabled;
    m2["direction"] = s2.direction;
    m2["tpd"] = s2.turnsPerDay;
    m2["activeHours"] = s2.activeHours;
    m2["rotationTime"] = s2.rotationTime;
    m2["restTime"] = s2.restTime;

    File file = LittleFS.open(SETTINGS_FILE, "w");
    if (!file) {
        Serial.println("Failed to open settings file for writing");
        return;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("Settings saved");
}
