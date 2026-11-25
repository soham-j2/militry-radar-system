# Military Radar System (ESP32 + WebSocket)

A simple IoT-based **Military Radar & Target Tracking System** built with an **ESP32**, ultrasonic & IR sensors, a servo motor, and a real-time **WebSocket web dashboard**.

## Description

This system scans a 180° area using an ultrasonic sensor mounted on a servo motor. When a PIR sensor detects motion, the radar automatically wakes up and starts scanning. If an object is detected within 40 cm, it triggers visual/audio alerts (Red LED + Buzzer) and sends live angle/distance data over WebSockets to a web dashboard. From the dashboard, you can lock onto a target to switch into **Tracking Mode**, where dual IR sensors keep the radar aimed at the moving object.

---

## Tech Stack

### Hardware
- **Microcontroller:** ESP32
- **Distance Sensor:** HC-SR04 Ultrasonic Sensor
- **Motion Wake-up:** PIR Motion Sensor
- **Target Tracking:** 2× IR Obstacle/Proximity Sensors (Left & Right)
- **Actuator:** Servo Motor (0°–180° sweep)
- **Alerts:** Active/Passive Buzzer, Green LED, Red LED

### Software & Libraries
- **Firmware:** C++ / Arduino (`militry Radar.ino`)
- **ESP32 Libraries:** `WiFi.h`, `WebSocketsServer.h`, `ArduinoJson.h`, `ESP32Servo.h`
- **Communication:** WebSockets (Port `81`) with JSON payloads
- **Dashboard:** HTML5 Canvas, CSS3, Vanilla JavaScript (`index.html`)

---

## Pin Configuration

| Component | ESP32 GPIO |
| :--- | :---: |
| Ultrasonic `TRIG` | `5` |
| Ultrasonic `ECHO` | `18` |
| Servo Motor | `13` |
| PIR Motion Sensor | `27` |
| IR Sensor (Left) | `34` |
| IR Sensor (Right) | `35` |
| Buzzer | `26` |
| Green LED (Active / Clear) | `14` |
| Red LED (Object Detected) | `12` |

---

## Features

- **Auto-Wake on Motion:** PIR sensor automatically activates scanning from standby.
- **180° Ultrasonic Sweep:** Measures distance every 3° and flags objects within 40 cm.
- **Target Lock & IR Tracking:** Lock onto a detected object from the dashboard; dual IR sensors follow the target left/right.
- **Real-Time Web Dashboard:** Live 180° radar sweep visualization, telemetry readout, alert log, and remote controls (`POWER`, `LOCK/UNLOCK`, `BUZZER`). Includes a built-in **Demo Mode** to preview without hardware.

---

## Setup & Usage

1. **Install Arduino Libraries** (via Arduino IDE Library Manager):
   - `WebSockets` by Markus Sattler
   - `ArduinoJson` by Benoit Blanchon
   - `ESP32Servo`
2. **Configure Wi-Fi:**
   Open `militry Radar.ino` and update `WIFI_SSID` and `WIFI_PASSWORD`.
3. **Upload & Get IP:**
   Upload the sketch to your ESP32 and open the Serial Monitor (`115200` baud) to see the assigned IP address (e.g., `ws://192.168.1.50:81`).
4. **Open the Dashboard:**
   Open `index.html` in any web browser, enter your ESP32's IP address, and click **Connect** (or click **Demo Mode** to test the UI offline).
