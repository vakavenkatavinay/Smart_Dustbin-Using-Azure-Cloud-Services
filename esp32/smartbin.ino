
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "mbedtls/md.h"
#include "mbedtls/base64.h"

// ==========================================
// 1. WIFI SETTINGS
// ==========================================

const char* WIFI_SSID = "Hotspot_username";
const char* WIFI_PASSWORD = "Password";

// ==========================================
// 2. AZURE IOT HUB SETTINGS
// ==========================================

const char* IOT_HUB_HOST =
  "Adinoinputecosort.azure-devices.net";

const char* DEVICE_ID = "SmartBin001";

const char* DEVICE_KEY =
  "SAS_key";

// DigiCert Global Root G2
const char* AZURE_ROOT_CA = R"EOF(certificate_data)EOF";

// ==========================================
// 3. HARDWARE PINS
// ==========================================

#define HAND_TRIG 25
#define HAND_ECHO 34

#define FILL_TRIG 26
#define FILL_ECHO 35
#define RED_LED_PIN 4

const int motorPins[4] = {32, 27, 33, 14};

const int motorSequence[8][4] = {
  {1,0,0,0},
  {1,1,0,0},
  {0,1,0,0},
  {0,1,1,0},
  {0,0,1,0},
  {0,0,1,1},
  {0,0,0,1},
  {1,0,0,1}
};

// ==========================================
// 4. SENSOR AND MOTOR SETTINGS
// ==========================================

const float HAND_LIMIT_CM = 15.0;
const float EMPTY_CM = 30.0;
const float FULL_CM = 5.0;

const int MOTOR_STEPS = 512;
const int MOTOR_DELAY_MS = 8;

const unsigned long HAND_LOST_DELAY = 500;
const unsigned long SENSOR_INTERVAL = 100;

// 15 seconds = 5760 routine messages/day
const unsigned long SEND_INTERVAL = 15000;

const unsigned long WIFI_RETRY_INTERVAL = 30000;
const unsigned long MQTT_RETRY_INTERVAL = 10000;

// ==========================================
// 5. GLOBAL VARIABLES
// ==========================================

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);

bool lidOpen = false;
bool handSeen = false;
bool handDetected = false;

float fillPercentage = 0;
bool fillValid = false;

unsigned long lastHandSeen = 0;
unsigned long lastSensorRead = 0;
unsigned long lastSend = 0;

unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;

String sasToken;
unsigned long sasExpiry = 0;

// ==========================================
// 6. URL ENCODING
// ==========================================

String urlEncode(const String& input) {
  String output = "";
  const char* hex = "0123456789ABCDEF";

  for (size_t i = 0; i < input.length(); i++) {
    unsigned char c = input[i];

    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' ||
        c == '.' || c == '~') {
      output += (char)c;
    } else {
      output += '%';
      output += hex[c >> 4];
      output += hex[c & 15];
    }
  }

  return output;
}

// ==========================================
// 7. AZURE SAS TOKEN
// ==========================================

bool createSasToken() {
  time_t now = time(nullptr);

  if (now < 1700000000) {
    Serial.println("Clock not synchronized");
    return false;
  }

  sasExpiry = (unsigned long)now + 3600;

  String resourceUri =
    String(IOT_HUB_HOST) + "/devices/" + DEVICE_ID;

  resourceUri.toLowerCase();

  String encodedUri = urlEncode(resourceUri);

  String toSign =
    encodedUri + "\n" + String(sasExpiry);

  unsigned char decodedKey[128];
  size_t decodedLength = 0;

  int result = mbedtls_base64_decode(
    decodedKey,
    sizeof(decodedKey),
    &decodedLength,
    (const unsigned char*)DEVICE_KEY,
    strlen(DEVICE_KEY)
  );

  if (result != 0) {
    Serial.println("Invalid device key encoding");
    return false;
  }

  unsigned char signature[32];

  const mbedtls_md_info_t* mdInfo =
    mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

  if (mdInfo == nullptr) return false;

  result = mbedtls_md_hmac(
    mdInfo,
    decodedKey,
    decodedLength,
    (const unsigned char*)toSign.c_str(),
    toSign.length(),
    signature
  );

  memset(decodedKey, 0, sizeof(decodedKey));

  if (result != 0) return false;

  unsigned char encodedSignature[128];
  size_t encodedLength = 0;

  result = mbedtls_base64_encode(
    encodedSignature,
    sizeof(encodedSignature),
    &encodedLength,
    signature,
    sizeof(signature)
  );

  if (result != 0) return false;

  String sig = "";

  for (size_t i = 0; i < encodedLength; i++) {
    sig += (char)encodedSignature[i];
  }

  sasToken =
    "SharedAccessSignature sr=" + encodedUri +
    "&sig=" + urlEncode(sig) +
    "&se=" + String(sasExpiry);

  return true;
}

// ==========================================
// 8. WIFI CONNECTION
// ==========================================

void connectWiFi() {
  Serial.println("Connecting to hotspot...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected yet");
  }
}

// ==========================================
// 9. TIME SYNCHRONIZATION
// ==========================================

bool syncClock() {
  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  Serial.println("Synchronizing clock...");

  for (int i = 0; i < 30; i++) {
    if (time(nullptr) > 1700000000) {
      Serial.println("Clock synchronized!");
      return true;
    }

    delay(500);
  }

  Serial.println("Clock sync failed");
  return false;
}

// ==========================================
// 10. AZURE MQTT CONNECTION
// ==========================================

void connectAzure() {
  if (WiFi.status() != WL_CONNECTED) return;

  time_t now = time(nullptr);

  if (now < 1700000000) {
    if (!syncClock()) return;
    now = time(nullptr);
  }

  if (sasToken.isEmpty() ||
      (unsigned long)now + 120 >= sasExpiry) {
    if (!createSasToken()) return;
  }

  String username =
    String(IOT_HUB_HOST) + "/" +
    DEVICE_ID + "/?api-version=2021-04-12";

  Serial.println("Connecting to Azure IoT Hub...");

  bool connected = mqtt.connect(
    DEVICE_ID,
    username.c_str(),
    sasToken.c_str()
  );

  if (connected) {
    Serial.println("AZURE CONNECTED!");
  } else {
    Serial.print("Azure MQTT failed, state: ");
    Serial.println(mqtt.state());
  }
}

// ==========================================
// 11. ULTRASONIC SENSOR
// ==========================================

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration =
    pulseIn(echoPin, HIGH, 30000UL);

  if (duration == 0) return -1;

  return duration * 0.0343 / 2.0;
}

// ==========================================
// 12. STEPPER MOTOR CONTROL
// ==========================================

void rotateMotor(bool clockwise) {
  for (int step = 0; step < MOTOR_STEPS; step++) {
    int index = clockwise ?
      step % 8 : 7 - (step % 8);

    for (int i = 0; i < 4; i++) {
      digitalWrite(
        motorPins[i],
        motorSequence[index][i]
      );
    }

    delay(MOTOR_DELAY_MS);
  }

  for (int i = 0; i < 4; i++) {
    digitalWrite(motorPins[i], LOW);
  }
}

// ==========================================
// 13. SENSOR AND LID LOGIC
// ==========================================

void updateSmartBin() {
  float handDistance =
    readDistance(HAND_TRIG, HAND_ECHO);

  handDetected =
    handDistance > 0 &&
    handDistance <= HAND_LIMIT_CM;

  if (handDetected) {
    handSeen = true;
    lastHandSeen = millis();
  }

  if (handDetected && !lidOpen) {
    Serial.println("Opening lid...");
    rotateMotor(true);
    lidOpen = true;
    Serial.println("Lid OPEN");
  }

  if (lidOpen && handSeen &&
      !handDetected &&
      millis() - lastHandSeen >= HAND_LOST_DELAY) {

    Serial.println("Closing lid...");
    rotateMotor(false);

    lidOpen = false;
    handSeen = false;

    Serial.println("Lid CLOSED");
  }

  delay(60);

  float fillDistance =
    readDistance(FILL_TRIG, FILL_ECHO);

  if (fillDistance > 0) {
    fillPercentage =
      (EMPTY_CM - fillDistance) /
      (EMPTY_CM - FULL_CM) * 100.0;

    fillPercentage =
      constrain(fillPercentage, 0.0f, 100.0f);

    fillValid = true;
  } else {
    fillValid = false;
  }
}

// ==========================================
// 14. SEND TELEMETRY TO AZURE
// ==========================================

void sendTelemetry() {
  if (!mqtt.connected()) return;

  JsonDocument doc;

  doc["deviceId"] = DEVICE_ID;

  if (fillValid) {
    doc["fillPercentage"] = fillPercentage;

    doc["binStatus"] =
      fillPercentage >= 90 ? "FULL" :
      fillPercentage >= 75 ? "NEAR FULL" :
                             "NORMAL";
  } else {
    doc["fillPercentage"] = nullptr;
    doc["binStatus"] = "UNKNOWN";
  }

  doc["handDetected"] = handDetected;
  doc["lidStatus"] = lidOpen ? "OPEN" : "CLOSED";

  String payload;
  serializeJson(doc, payload);

  String topic =
    String("devices/") + DEVICE_ID +
    "/messages/events/";

  bool sent = mqtt.publish(
    topic.c_str(),
    payload.c_str()
  );

  if (sent) {
    Serial.println("Telemetry published:");
    Serial.println(payload);
  } else {
    Serial.println("Telemetry publish failed");
  }
}

// ==========================================
// 15. SETUP
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, LOW);

  pinMode(HAND_TRIG, OUTPUT);
  pinMode(HAND_ECHO, INPUT);

  pinMode(FILL_TRIG, OUTPUT);
  pinMode(FILL_ECHO, INPUT);

  digitalWrite(HAND_TRIG, LOW);
  digitalWrite(FILL_TRIG, LOW);

  for (int i = 0; i < 4; i++) {
    pinMode(motorPins[i], OUTPUT);
    digitalWrite(motorPins[i], LOW);
  }

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    syncClock();
  }

  secureClient.setCACert(AZURE_ROOT_CA);
  secureClient.setTimeout(10);

  mqtt.setServer(IOT_HUB_HOST, 8883);
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(10);

  Serial.println("EcoSort Smart Bin Ready!");
}

// ==========================================
// 16. MAIN LOOP
// ==========================================

void loop() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;

  if (fillValid && fillPercentage >= 90.0) {

  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState = !ledState;

    digitalWrite(RED_LED_PIN, ledState ? HIGH : LOW);
  }

  } else {
  ledState = false;
  digitalWrite(RED_LED_PIN, LOW);
  }

  // Wi-Fi reconnect
  if (WiFi.status() != WL_CONNECTED) {

    if (millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
      lastWiFiAttempt = millis();

      Serial.println("Retrying WiFi...");

      WiFi.disconnect(false, false);
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

  } else {

    // Azure MQTT reconnect
    if (!mqtt.connected() &&
        millis() - lastMQTTAttempt >= MQTT_RETRY_INTERVAL) {

      lastMQTTAttempt = millis();
      connectAzure();
    }

    if (mqtt.connected()) {
      mqtt.loop();

      if ((unsigned long)time(nullptr) + 120
          >= sasExpiry) {
        mqtt.disconnect();
      }
    }
  }

  // Sensor reading
  if (millis() - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = millis();
    updateSmartBin();
  }

  // Send telemetry
  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    sendTelemetry();
  }
}
