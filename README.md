# Watch Winder Controller

An ESP8266-based automatic watch winder with a web interface for controlling two independent watch winders.

## Features

- Control 2 watch winders independently
- Mobile-friendly web interface
- Configurable turns per day (TPD) for different watch movements
- Multiple rotation modes: Clockwise, Counter-clockwise, Bidirectional
- Automatic scheduling with configurable active hours and rest periods
- WiFi setup via captive portal (no hardcoding credentials)
- Settings persist across power cycles

## Hardware Requirements

### Components

| Component | Quantity | Description |
|-----------|----------|-------------|
| ESP8266 NodeMCU | 1 | ESP-12E module with CP2102 USB (DevKit v1.0) |
| 28BYJ-48 Stepper Motor | 2 | 5V unipolar stepper motor |
| ULN2003 Driver Board | 2 | Stepper motor driver module |
| 5V Power Supply | 1 | 2A minimum, for motors only (not ESP8266) |
| USB Cable | 1 | Micro-USB for ESP8266 power |
| Jumper Wires | ~20 | Female-to-female for connections |

### Specifications

**ESP8266 NodeMCU:**
- Operating Voltage: 3.3V (5V tolerant via USB/VIN)
- Digital I/O Pins: 11 (sufficient for 2 motors)
- Built-in WiFi: 802.11 b/g/n

**28BYJ-48 Stepper Motor:**
- Operating Voltage: 5V DC
- Step Angle: 5.625° (64 steps per revolution)
- Gear Reduction Ratio: 1:64
- Steps per Revolution: 2048 (full-step) / 4096 (half-step)
- Current Draw: ~240mA per motor

## Wiring Diagram

```
                                 5V Power Supply (2A+)
                                 ┌─────────────────┐
                                 │  +5V        GND │
                                 └──┬──────────┬──┘
                                    │          │
                    ┌───────────────┴───┐      │
                    │                   │      │
                    ▼                   ▼      │
ESP8266 NodeMCU              ULN2003 #1 (Motor 1)         ULN2003 #2 (Motor 2)
┌─────────────────┐          ┌─────────────────┐          ┌─────────────────┐
│                 │          │                 │          │                 │
│ D1 (GPIO5)  ────┼──────────┼─► IN1           │          │                 │
│ D2 (GPIO4)  ────┼──────────┼─► IN2           │          │                 │
│ D3 (GPIO0)  ────┼──────────┼─► IN3           │          │                 │
│ D4 (GPIO2)  ────┼──────────┼─► IN4           │          │                 │
│                 │          │                 │          │                 │
│ D5 (GPIO14) ────┼──────────┼─────────────────┼──────────┼─► IN1           │
│ D6 (GPIO12) ────┼──────────┼─────────────────┼──────────┼─► IN2           │
│ D7 (GPIO13) ────┼──────────┼─────────────────┼──────────┼─► IN3           │
│ D8 (GPIO15) ────┼──────────┼─────────────────┼──────────┼─► IN4           │
│                 │          │                 │          │                 │
│                 │          │      VCC ◄──────┼──5V──────┼─► VCC           │
│ GND         ────┼──────────┼─► GND ──────────┼──────────┼─► GND ◄─────────┼── GND
│                 │          │                 │          │                 │  (common)
└─────────────────┘          └─────────────────┘          └─────────────────┘

ESP8266 powered separately via USB
```

**Important: Common Ground** - The GND from the ESP8266, both ULN2003 boards, and the 5V power supply must all be connected together.

### Pin Reference Table

| ESP8266 Pin | GPIO | Motor 1 | Motor 2 |
|-------------|------|---------|---------|
| D1 | GPIO5 | IN1 | - |
| D2 | GPIO4 | IN2 | - |
| D3 | GPIO0 | IN3 | - |
| D4 | GPIO2 | IN4 | - |
| D5 | GPIO14 | - | IN1 |
| D6 | GPIO12 | - | IN2 |
| D7 | GPIO13 | - | IN3 |
| D8 | GPIO15 | - | IN4 |
| GND | GND | GND | GND |

### Power Considerations

**Use separate power supplies for the ESP8266 and motors:**

| Component | Power Source | Notes |
|-----------|--------------|-------|
| ESP8266 | USB cable | 5V via micro-USB port |
| ULN2003 boards | External 5V 2A PSU | Dedicated motor power |

**Why separate power?**
- Each 28BYJ-48 motor draws ~240mA under load
- Two motors = ~500mA total, plus driver overhead
- The ESP8266's onboard regulator cannot safely supply this current
- Sharing power can cause voltage drops, WiFi disconnects, or resets

**Critical: Common Ground**
- All GND connections must be tied together
- ESP8266 GND ↔ ULN2003 GND ↔ Power Supply GND
- Without common ground, the control signals won't work

## Software Setup

### Prerequisites

1. **VS Code** with **PlatformIO IDE** extension
   - Install VS Code: https://code.visualstudio.com/
   - Install PlatformIO extension from VS Code marketplace

   OR

2. **PlatformIO Core (CLI)**
   ```bash
   pip install platformio
   ```

### Project Structure

```
watch_winder/
├── platformio.ini          # Build configuration
├── src/
│   └── main.cpp            # Main firmware
├── include/
│   ├── config.h            # Pin definitions & defaults
│   ├── stepper.h           # Stepper motor control class
│   └── scheduler.h         # TPD scheduling logic
├── data/                   # Web interface (LittleFS)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── README.md
```

## Flashing Instructions

### 1. Open the Project in VS Code

1. Open VS Code
2. Click **File → Open Folder**
3. Navigate to the `watch_winder` folder and click **Open**
4. PlatformIO will automatically detect `platformio.ini` and initialize the project
5. Wait for PlatformIO to finish downloading dependencies (check bottom status bar)

### 2. Connect the ESP8266

Plug the NodeMCU into your computer via USB cable.

### 3. Build and Upload Firmware

**Option A: Using VS Code toolbar** (bottom of window)

| Icon | Action |
|------|--------|
| ✓ (checkmark) | Build/compile the project |
| → (right arrow) | Upload firmware to ESP8266 |
| 🔌 (plug) | Open serial monitor |

**Option B: Using PlatformIO terminal**

Open terminal: **PlatformIO sidebar → Quick Access → New Terminal**

```bash
# Build the project
pio run

# Upload firmware to ESP8266
pio run --target upload
```

### 4. Upload Web Interface

The HTML/CSS/JS files must be uploaded separately to the LittleFS filesystem.

**Option A: Using VS Code**

1. Click the **PlatformIO icon** in the left sidebar (alien head)
2. Expand **PROJECT TASKS → nodemcu**
3. Click **Upload Filesystem Image**

**Option B: Using terminal**

```bash
pio run --target uploadfs
```

### 5. Open Serial Monitor

**Option A:** Click the **plug icon** 🔌 in the bottom toolbar

**Option B:** Run in terminal:
```bash
pio device monitor
```

**Expected output on first boot:**
```
=================================
  Watch Winder Controller v1.0
=================================

Motors initialized
No settings file found, using defaults
Starting Access Point...
AP started: WatchWinder-Setup
AP IP address: 192.168.4.1
Web server started

System ready!
```

## First-Time WiFi Setup

1. **Power on** the ESP8266
2. **Connect** to the WiFi network `WatchWinder-Setup` (open network)
3. A **captive portal** should open automatically
   - If not, open a browser and go to `http://192.168.4.1`
4. **Select your home WiFi** network from the dropdown
5. **Enter the password** and click Connect
6. The device will **reboot** and connect to your home network
7. Check the **serial monitor** for the new IP address
8. Access the web interface at `http://<device-ip>`

## Web Interface

### Dashboard Features

- **Global Controls**: Start/Stop all motors
- **Per-Motor Controls**:
  - Start/Stop individual motors
  - Test button for manual rotation
- **Status Display**:
  - Current cycle progress
  - Turns completed today
  - Time until next cycle
- **Settings** (per motor):
  - Enable/Disable
  - Direction (CW/CCW/Bidirectional)
  - Turns Per Day (TPD)
  - Active Hours
  - Rotation Time (seconds)
  - Rest Time (minutes)

### Configuration Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Turns Per Day (TPD) | 100-2000 | 650 | Total rotations per day |
| Active Hours | 1-24 | 12 | Hours the winder operates |
| Rotation Time | 1-60 sec | 10 | Duration of each rotation burst |
| Rest Time | 1-60 min | 5 | Pause between rotations |
| Direction | CW/CCW/Bi | CW | Rotation direction |

### TPD Recommendations by Watch Brand

| Brand | Recommended TPD | Direction |
|-------|-----------------|-----------|
| Rolex | 650 | Clockwise |
| Omega | 800 | Clockwise |
| Tag Heuer | 800 | Clockwise |
| Breitling | 800 | Clockwise |
| Seiko | 650 | Bidirectional |
| Cartier | 650 | Clockwise |
| Panerai | 630 | Clockwise |
| IWC | 800 | Both directions |

*Note: Check your watch's manual for specific requirements.*

## API Reference

The firmware exposes a REST API for programmatic control:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Get current status of both motors |
| `/api/settings` | GET | Get current settings |
| `/api/settings` | POST | Update settings (JSON body) |
| `/api/start` | POST | Start motors (`{"motor": 0/1/2}`) |
| `/api/stop` | POST | Stop motors (`{"motor": 0/1/2}`) |
| `/api/test` | POST | Test motor (`{"motor": 1/2, "direction": 0/1/2, "duration": 3}`) |
| `/api/wifi/scan` | GET | Scan available WiFi networks |
| `/api/wifi/connect` | POST | Connect to WiFi (`{"ssid": "...", "password": "..."}`) |

### Example API Usage

```bash
# Get status
curl http://192.168.1.100/api/status

# Start motor 1
curl -X POST http://192.168.1.100/api/start -H "Content-Type: application/json" -d '{"motor": 1}'

# Update settings
curl -X POST http://192.168.1.100/api/settings -H "Content-Type: application/json" -d '{
  "motor1": {"enabled": true, "direction": 0, "tpd": 650, "activeHours": 12, "rotationTime": 10, "restTime": 5}
}'
```

## Troubleshooting

### Motor not spinning

1. Check wiring connections match the pin diagram
2. Verify 5V power supply is connected to ULN2003 VCC pins
3. Ensure common ground between ESP8266 and motor power supply
4. Ensure adequate power supply (2A minimum for both motors)
5. Test with the "Test" button in the web interface

### Cannot connect to WiFi AP

1. Ensure no saved credentials exist (re-flash to clear)
2. Try connecting from a different device
3. Check serial monitor for error messages

### Web interface not loading

1. Verify `pio run --target uploadfs` was run
2. Check serial monitor for "Web server started" message
3. Clear browser cache and try again

### ESP8266 keeps resetting or WiFi disconnects

1. **Do not power motors through ESP8266** - use separate 5V supply for ULN2003 boards
2. Check that motor power supply provides at least 2A
3. Ensure common ground is connected properly
4. Try a different USB cable or power source for ESP8266

### Motors running slowly or skipping

1. Increase `STEP_DELAY_MS` in `config.h` (default: 2ms)
2. Check for mechanical binding in the winder mechanism
3. Ensure power supply provides sufficient current

### Settings not saving

1. Check serial monitor for "Settings saved" confirmation
2. LittleFS may need reformatting - re-upload filesystem
3. Verify free flash space with `pio run --target size`

## Customization

### Changing Pin Assignments

Edit `include/config.h`:

```cpp
#define MOTOR1_IN1 D1  // Change to desired pin
#define MOTOR1_IN2 D2
// ...
```

### Adjusting Motor Speed

In `include/config.h`:

```cpp
#define STEP_DELAY_MS 2  // Lower = faster, higher = slower
```

### Changing Default Settings

In `include/config.h`:

```cpp
#define DEFAULT_TPD 650
#define DEFAULT_ACTIVE_HOURS 12
#define DEFAULT_ROTATION_TIME 10
#define DEFAULT_REST_TIME 5
```

## License

MIT License - Feel free to modify and distribute.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## Acknowledgments

- ESP8266 Arduino Core: https://github.com/esp8266/Arduino
- ArduinoJson: https://arduinojson.org/
- PlatformIO: https://platformio.org/
