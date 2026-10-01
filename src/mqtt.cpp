#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>

#include <device_id.h>

// The board connects the DHT22 data line to ESP-01S GPIO2.
constexpr uint8_t kDhtPin = 2;
constexpr unsigned long kDhtReadSpacingMs = 2100UL;
constexpr unsigned long kMeasurementIntervalMs = 1UL * 60UL * 1000UL;
constexpr unsigned long kWifiRetryMs = 10UL * 1000UL;
constexpr unsigned long kMqttRetryMs = 5UL * 1000UL;
constexpr unsigned long kPublishRetryMs = 5UL * 1000UL;

DHT dht(kDhtPin, DHT22);
WiFiClient networkClient;
MQTTClient mqttClient(256);

String clientId;
String stateTopic;
unsigned long lastMeasurementMs = 0;
unsigned long lastDhtReadMs = 0;
unsigned long lastWifiAttemptMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastPublishAttemptMs = 0;
float lastHumidity = NAN;
float lastTemperatureC = NAN;
bool publishPending = false;
bool mqttAttempted = false;
bool dhtReadOccurred = false;

void waitForDhtRead() {
  if (dhtReadOccurred) {
    const unsigned long elapsed = millis() - lastDhtReadMs;
    if (elapsed < kDhtReadSpacingMs) {
      delay(kDhtReadSpacingMs - elapsed);
    }
  }
  lastDhtReadMs = millis();
  dhtReadOccurred = true;
}

void readMeasurement() {
  // The AM2302 returns the previous measurement. Prime it, then use a
  // second physical read after the sensor and DHT library are ready.
  waitForDhtRead();
  dht.readHumidity();
  waitForDhtRead();
  const float humidity = dht.readHumidity();
  const float temperatureC = dht.readTemperature();
  lastMeasurementMs = millis();

  if (isnan(humidity) || isnan(temperatureC)) {
    publishPending = false;
    return;
  }

  lastHumidity = humidity;
  lastTemperatureC = temperatureC;
  publishPending = true;
}

void connectMqtt() {
  lastMqttAttemptMs = millis();
  mqttAttempted = true;

  bool connected = false;
  if (sensor_config::mqtt_username[0] || sensor_config::mqtt_password[0]) {
    connected = mqttClient.connect(clientId.c_str(),
                                   sensor_config::mqtt_username,
                                   sensor_config::mqtt_password);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (connected) {
    // Publish a fresh reading after reconnecting.
    readMeasurement();
    lastPublishAttemptMs = 0;
  }
}

void publishMeasurement() {
  lastPublishAttemptMs = millis();
  String payload = F("{\"temperature_c\":");
  payload += String(lastTemperatureC, 1);
  payload += F(",\"humidity_pct\":");
  payload += String(lastHumidity, 1);
  payload += '}';

  if (mqttClient.publish(stateTopic.c_str(), payload.c_str(), true, 1)) {
    publishPending = false;
  }
}

void setup() {
  dht.begin();
  delay(kDhtReadSpacingMs);  // Wait more than 2 s after sensor power-up.
  readMeasurement();

  clientId = String(F("esp01-")) + SENSOR_ID;
  stateTopic = String(sensor_config::mqtt_topic_prefix);
  if (stateTopic.endsWith("/")) {
    stateTopic.remove(stateTopic.length() - 1);
  }
  stateTopic += '/';
  stateTopic += SENSOR_ID;
  stateTopic += F("/state");

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(clientId);
  WiFi.begin(sensor_config::wifi_ssid, sensor_config::wifi_password);
  lastWifiAttemptMs = millis();

  mqttClient.begin(sensor_config::mqtt_host, sensor_config::mqtt_port,
                   networkClient);
  mqttClient.setOptions(30, true, 1000);
}

void loop() {
  const unsigned long now = millis();
  if (now - lastMeasurementMs >= kMeasurementIntervalMs) {
    readMeasurement();
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiAttemptMs >= kWifiRetryMs) {
      WiFi.begin(sensor_config::wifi_ssid, sensor_config::wifi_password);
      lastWifiAttemptMs = millis();
    }
    delay(10);
    return;
  }

  if (!mqttClient.connected()) {
    if (!mqttAttempted || now - lastMqttAttemptMs >= kMqttRetryMs) {
      connectMqtt();
    }
    delay(10);
    return;
  }

  mqttClient.loop();
  if (publishPending &&
      (lastPublishAttemptMs == 0 ||
       now - lastPublishAttemptMs >= kPublishRetryMs)) {
    publishMeasurement();
  }
  delay(10);
}
