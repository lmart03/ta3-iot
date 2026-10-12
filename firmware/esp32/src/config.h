#pragma once
#include <Arduino.h>

// Parametros sobreescribibles desde platformio.ini (build_flags)
#ifndef DEMO_MODE
#define DEMO_MODE 1
#endif
#ifndef PUBLISH_INTERVAL_MS
#define PUBLISH_INTERVAL_MS 1000
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "Wokwi-GUEST"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MQTT_HOST
#define MQTT_HOST "host.wokwi.internal"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_TOPIC_BASE
#define MQTT_TOPIC_BASE "smart-environment/esp32-001"
#endif

namespace Config {
  constexpr char DEVICE_ID[] = "ESP32-001";

  // GPIO originales (no modificar)
  constexpr uint8_t DHT_PIN = 15;
  constexpr uint8_t LDR_PIN = 34;
  constexpr uint8_t PIR_PIN = 27;

  constexpr unsigned long INTERVAL_MS = PUBLISH_INTERVAL_MS;
  constexpr unsigned long CYCLE_MS = 300000;  // escenario completo: 5 min
  constexpr bool DEMO = DEMO_MODE != 0;

  constexpr float WARNING_TEMP = 30.0f;
  constexpr float CRITICAL_TEMP = 35.0f;

  constexpr unsigned long WIFI_RETRY_MS = 10000;
  constexpr unsigned long MQTT_RETRY_MS = 5000;
  constexpr uint8_t WIFI_CHANNEL = 6;  // Wokwi-GUEST: evita el escaneo (~4 s)

  constexpr char TOPIC_TELEMETRY[] = MQTT_TOPIC_BASE "/telemetry";
  constexpr char TOPIC_STATUS[] = MQTT_TOPIC_BASE "/status";
}
