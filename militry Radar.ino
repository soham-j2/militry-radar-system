// ============================================
// MILITARY RADAR SYSTEM - WebSocket Version
// ============================================

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// ============================================
// YOUR SETTINGS
// ============================================
const char* WIFI_SSID     = "OPPO A17";
const char* WIFI_PASSWORD = "*******#";

// ============================================
// PIN DEFINITIONS
// ============================================
#define PIN_TRIG        5
#define PIN_ECHO        18
#define PIN_SERVO       13
#define PIN_PIR         27
#define PIN_IR_LEFT     34
#define PIN_IR_RIGHT    35
#define PIN_BUZZER      26
#define PIN_LED_GREEN   14
#define PIN_LED_RED     12

// ============================================
// SYSTEM VARIABLES
// ============================================
bool systemActive   = false;
bool targetLocked   = false;
bool buzzerEnabled  = true;
bool objectDetected = false;

int  currentAngle   = 0;
int  sweepDirection = 1;

#define OBJECT_THRESHOLD  40
#define BUZZER_FREQ       2000
#define SWEEP_DELAY       5
#define PUBLISH_DELAY     30

unsigned long lastSweepTime   = 0;
unsigned long lastPublishTime = 0;
unsigned long lastBuzzerTime  = 0;
bool buzzerState = false;

// ============================================
// OBJECTS
// ============================================
Servo           radarServo;
WebSocketsServer webSocket(81); // WebSocket on port 81

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  Serial.println("=== MILITARY RADAR SYSTEM - WebSocket ===");

  // Pin setup
  pinMode(PIN_TRIG,      OUTPUT);
  pinMode(PIN_ECHO,      INPUT);
  pinMode(PIN_PIR,       INPUT);
  pinMode(PIN_IR_LEFT,   INPUT);
  pinMode(PIN_IR_RIGHT,  INPUT);
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);

  digitalWrite(PIN_BUZZER,    LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED,   LOW);

  // Servo
  radarServo.attach(PIN_SERVO);
  radarServo.write(0);
  delay(500);

  // Connect WiFi
  connectWiFi();

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81!");
  Serial.print("Dashboard URL: ws://");
  Serial.print(WiFi.localIP());
  Serial.println(":81");
  Serial.println("=== SYSTEM READY ===");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  webSocket.loop(); // Handle WebSocket events

  checkPIR();

  if (systemActive) {
    if (!targetLocked) {
      scanningMode();
    } else {
      trackingMode();
    }
  }

  handleBuzzer();
}

// ============================================
// WEBSOCKET EVENT HANDLER
// ============================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch(type) {

    case WStype_CONNECTED:
      Serial.printf("Client #%u connected!\n", num);
      // Send current status to newly connected client
      sendStatus(num);
      break;

    case WStype_DISCONNECTED:
      Serial.printf("Client #%u disconnected!\n", num);
      break;

    case WStype_TEXT:
      // Receive command from dashboard
      handleCommand((char*)payload, num);
      break;

    default:
      break;
  }
}

// ============================================
// HANDLE COMMANDS FROM DASHBOARD
// ============================================
void handleCommand(String msg, uint8_t clientNum) {
  Serial.print("Command received: ");
  Serial.println(msg);

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.println("JSON parse error!");
    return;
  }

  String cmd = doc["cmd"].as<String>();

  // --- LOCK / UNLOCK ---
  if (cmd == "LOCK" && objectDetected) {
    targetLocked = true;
    beepBuzzer(2);
    broadcastJson("mode",  "TRACKING");
    broadcastJson("alert", "TARGET_LOCKED");
    Serial.println("TARGET LOCKED!");
  }
  else if (cmd == "UNLOCK") {
    targetLocked   = false;
    objectDetected = false;
    digitalWrite(PIN_LED_RED,   LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);
    broadcastJson("mode",  "SCANNING");
    broadcastJson("alert", "UNLOCKED");
    beepBuzzer(1);
    Serial.println("UNLOCKED");
  }

  // --- POWER ---
  else if (cmd == "POWER_ON") {
    systemActive = true;
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED,   LOW);
    broadcastJson("status", "ACTIVE");
    broadcastJson("mode",   "SCANNING");
    Serial.println("SYSTEM ON");
  }
  else if (cmd == "POWER_OFF") {
    systemActive   = false;
    targetLocked   = false;
    objectDetected = false;
    radarServo.write(0);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED,   LOW);
    broadcastJson("status", "STANDBY");
    broadcastJson("mode",   "STANDBY");
    Serial.println("SYSTEM OFF");
  }

  // --- BUZZER ---
  else if (cmd == "BUZZER_ON") {
    buzzerEnabled = true;
    Serial.println("BUZZER ON");
  }
  else if (cmd == "BUZZER_OFF") {
    buzzerEnabled = false;
    noTone(PIN_BUZZER);
    Serial.println("BUZZER OFF");
  }
}

// ============================================
// PIR CHECK
// ============================================
void checkPIR() {
  int pirValue = digitalRead(PIN_PIR);
  if (pirValue == HIGH && !systemActive) {
    delay(100);
    if (digitalRead(PIN_PIR) == HIGH) {
      Serial.println("PIR: Motion confirmed! Activating...");
      systemActive = true;
      digitalWrite(PIN_LED_GREEN, HIGH);
      digitalWrite(PIN_LED_RED,   LOW);
      broadcastJson("status", "ACTIVE");
      broadcastJson("mode",   "SCANNING");
      beepBuzzer(1);
    }
  }
}

// ============================================
// SCANNING MODE
// ============================================
void scanningMode() {
  unsigned long now = millis();

  if (now - lastSweepTime >= SWEEP_DELAY) {
    lastSweepTime = now;

    currentAngle += sweepDirection;
    if (currentAngle >= 180) sweepDirection = -1;
    if (currentAngle <= 0)   sweepDirection =  1;

    radarServo.write(currentAngle);

    // Measure every 3 degrees
    float distance = -1;
    if (currentAngle % 3 == 0) {
      distance = measureDistance();
    }

    // Publish data
    if (now - lastPublishTime >= PUBLISH_DELAY) {
      lastPublishTime = now;
      sendRadarData(currentAngle, distance);
    }

    // Object detection
    if (distance > 0 && distance < OBJECT_THRESHOLD) {
      if (!objectDetected) {
        objectDetected = true;
        Serial.printf("OBJECT! Angle:%d Dist:%.1f\n", currentAngle, distance);
        broadcastJson("alert", "OBJECT_DETECTED");
        digitalWrite(PIN_LED_RED,   HIGH);
        digitalWrite(PIN_LED_GREEN, LOW);
      }
    } else if (distance > 0) {
      if (objectDetected) {
        objectDetected = false;
        broadcastJson("alert", "CLEAR");
        digitalWrite(PIN_LED_RED,   LOW);
        digitalWrite(PIN_LED_GREEN, HIGH);
      }
    }
  }
}

// ============================================
// TRACKING MODE
// ============================================
void trackingMode() {
  int irLeft  = digitalRead(PIN_IR_LEFT);
  int irRight = digitalRead(PIN_IR_RIGHT);

  float distance = measureDistance();
  sendRadarData(currentAngle, distance);

  // Send IR states
  StaticJsonDocument<128> doc;
  doc["ir_left"]  = (irLeft  == LOW) ? 1 : 0;
  doc["ir_right"] = (irRight == LOW) ? 1 : 0;
  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);

  if (irLeft == LOW && irRight == HIGH) {
    currentAngle -= 2;
    currentAngle = constrain(currentAngle, 0, 180);
    radarServo.write(currentAngle);
    delay(20);
  }
  else if (irRight == LOW && irLeft == HIGH) {
    currentAngle += 2;
    currentAngle = constrain(currentAngle, 0, 180);
    radarServo.write(currentAngle);
    delay(20);
  }
  else if (irLeft == HIGH && irRight == HIGH) {
    broadcastJson("alert", "TARGET_LOST");
  }
}

// ============================================
// SEND RADAR DATA (angle + distance together)
// ============================================
void sendRadarData(int angle, float distance) {
  StaticJsonDocument<128> doc;
  doc["angle"]    = angle;
  doc["distance"] = distance;
  doc["mode"]     = targetLocked ? "TRACKING" : "SCANNING";
  doc["status"]   = systemActive ? "ACTIVE" : "STANDBY";
  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}

// ============================================
// SEND CURRENT STATUS TO NEW CLIENT
// ============================================
void sendStatus(uint8_t num) {
  StaticJsonDocument<128> doc;
  doc["status"] = systemActive ? "ACTIVE"   : "STANDBY";
  doc["mode"]   = targetLocked ? "TRACKING" : "SCANNING";
  doc["angle"]  = currentAngle;
  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(num, out);
}

// ============================================
// BROADCAST SIMPLE KEY-VALUE JSON
// ============================================
void broadcastJson(String key, String value) {
  StaticJsonDocument<128> doc;
  doc[key] = value;
  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}

// ============================================
// DISTANCE MEASUREMENT
// ============================================
float measureDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 15000);
  if (duration == 0) return -1;
  return (duration * 0.0343) / 2.0;
}

// ============================================
// BUZZER
// ============================================
void handleBuzzer() {
  if (!buzzerEnabled || !objectDetected) {
    noTone(PIN_BUZZER);
    return;
  }
  unsigned long now = millis();
  if (now - lastBuzzerTime >= 500) {
    lastBuzzerTime = now;
    buzzerState = !buzzerState;
    buzzerState ? tone(PIN_BUZZER, BUZZER_FREQ) : noTone(PIN_BUZZER);
  }
}

void beepBuzzer(int times) {
  for (int i = 0; i < times; i++) {
    tone(PIN_BUZZER, BUZZER_FREQ);
    delay(200);
    noTone(PIN_BUZZER);
    delay(200);
  }
}

// ============================================
// WIFI
// ============================================
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi CONNECTED!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}
