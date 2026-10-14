#include <Arduino.h>

#include "config.h"
#include "network.h"
#include "payload.h"
#include "sensors.h"

// ========================================
// SMART ENVIRONMENT IoT - ESP32
// Sensores (DHT22, LDR, PIR) -> WiFi -> MQTT (EMQX)
// ========================================

unsigned long lastRead = 0;
uint32_t sequence = 0;

void publishMeasurement() {
  SensorData data = acquire();

  if (!validate(data)) {
    Serial.println("[ERROR] Lectura invalida, no se publica");
    return;
  }

  sequence++;
  const char* status = temperatureStatus(data.temperature);
  const char* source = Config::DEMO ? "synthetic" : "sensor";

  char payload[384];
  int length = serializePayload(payload, sizeof(payload), data, status, source, sequence);

  if (length < 0 || length >= (int)sizeof(payload)) {
    Serial.println("[ERROR] JSON truncado");
    return;
  }

  bool sent = publishTelemetry(payload);
  printMeasurement(data, status, source, payload, sent);
}

void setup() {
  Serial.begin(115200);

  setupSensors();

  Serial.println();
  Serial.println("================================");
  Serial.println("    SMART ENVIRONMENT IoT");
  Serial.println("================================");
  Serial.printf("Dispositivo: %s\r\n", Config::DEVICE_ID);
  Serial.printf("Modo: %s\r\n", Config::DEMO ? "SIMULACION AUTOMATICA" : "SENSORES WOKWI");
  Serial.printf("Intervalo: %lu ms\r\n", Config::INTERVAL_MS);
  Serial.println("Ciclo demo: 5 minutos");
  Serial.printf("Broker: %s:%d\r\n", MQTT_HOST, MQTT_PORT);
  Serial.println("================================");

  setupNetwork();
  lastRead = millis();
}

void loop() {
  maintainNetwork();

  unsigned long now = millis();
  if (now - lastRead >= Config::INTERVAL_MS) {
    lastRead += Config::INTERVAL_MS;
    publishMeasurement();
  }
}
