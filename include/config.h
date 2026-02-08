#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration
// ============================================

// Your home WiFi credentials (leave empty for captive portal setup)
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Access Point settings for initial setup
#define AP_SSID "WatchWinder-Setup"
#define AP_PASSWORD ""  // Empty = open network

// WiFi connection timeout (milliseconds)
#define WIFI_TIMEOUT 15000

// ============================================
// Motor 1 Pin Definitions (ULN2003 #1)
// ============================================
// Control signals only - ULN2003 VCC powered by separate 5V supply
#define MOTOR1_IN1 D1  // GPIO5
#define MOTOR1_IN2 D2  // GPIO4
#define MOTOR1_IN3 D3  // GPIO0
#define MOTOR1_IN4 D4  // GPIO2

// ============================================
// Motor 2 Pin Definitions (ULN2003 #2)
// ============================================
// Control signals only - ULN2003 VCC powered by separate 5V supply
#define MOTOR2_IN1 D5  // GPIO14
#define MOTOR2_IN2 D6  // GPIO12
#define MOTOR2_IN3 D7  // GPIO13
#define MOTOR2_IN4 D8  // GPIO15

// ============================================
// Power Configuration Notes
// ============================================
// - ESP8266: Powered via USB (do NOT power motors through ESP8266)
// - ULN2003 boards: Powered by separate 5V 2A power supply
// - Common ground required between ESP8266, ULN2003s, and 5V PSU

// ============================================
// Stepper Motor Constants (28BYJ-48)
// ============================================
#define STEPS_PER_REVOLUTION 2038  // Full steps (gear ratio 63.68395:1)
#define HALF_STEPS_PER_REVOLUTION 4076  // Half steps per revolution
#define STEP_DELAY_MS 2            // Delay between steps (~7 RPM, max safe ~15 RPM)

// ============================================
// Default Motor Settings
// ============================================
#define DEFAULT_TPD 650              // Turns per day
#define DEFAULT_ACTIVE_HOURS 12      // Hours of operation per day
#define DEFAULT_ROTATION_TIME 10     // Seconds per rotation burst
#define DEFAULT_REST_TIME 5          // Minutes between rotations
#define DEFAULT_DIRECTION 0          // 0=CW, 1=CCW, 2=Bidirectional

// ============================================
// Web Server
// ============================================
#define WEB_SERVER_PORT 80
#define DNS_PORT 53

// ============================================
// Storage
// ============================================
#define SETTINGS_FILE "/settings.json"

#endif // CONFIG_H
