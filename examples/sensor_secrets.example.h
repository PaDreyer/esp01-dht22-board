#pragma once

#include <stdint.h>

// Copy this template to include/sensor_secrets.h and fill in your own values.
// Never commit that local file.
namespace sensor_config {
constexpr char wifi_ssid[] = "YOUR_WIFI_SSID";
constexpr char wifi_password[] = "YOUR_WIFI_PASSWORD";

constexpr char mqtt_host[] = "192.168.1.10";
constexpr uint16_t mqtt_port = 1883;
constexpr char mqtt_username[] = "YOUR_MQTT_USERNAME";
constexpr char mqtt_password[] = "YOUR_MQTT_PASSWORD";
constexpr char mqtt_topic_prefix[] = "home/sensors";
}  // namespace sensor_config
