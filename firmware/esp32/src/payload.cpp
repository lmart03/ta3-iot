#include "payload.h"

#include "config.h"

int serializePayload(char* out, size_t size, const SensorData& d,
                     const char* status, const char* source, uint32_t seq) {
  return snprintf(out, size,
                  "{"
                  "\"device_id\":\"%s\","
                  "\"temperature\":%.2f,"
                  "\"humidity\":%.2f,"
                  "\"light_raw\":%d,"
                  "\"motion\":%s,"
                  "\"status\":\"%s\","
                  "\"source\":\"%s\","
                  "\"sequence\":%lu,"
                  "\"uptime_ms\":%lu"
                  "}",
                  Config::DEVICE_ID, d.temperature, d.humidity, d.lightRaw,
                  d.motion ? "true" : "false", status, source,
                  (unsigned long)seq, millis());
}

void printMeasurement(const SensorData& d, const char* status,
                      const char* source, const char* payload, bool sent) {
  Serial.println();
  Serial.println("===== SMART ENVIRONMENT =====");
  Serial.printf("Temperatura: %.2f C\n", d.temperature);
  Serial.printf("Humedad: %.2f %%\n", d.humidity);
  Serial.printf("Iluminacion ADC: %d\n", d.lightRaw);
  Serial.printf("Movimiento: %s\n", d.motion ? "DETECTADO" : "NO");
  Serial.printf("Estado: %s\n", status);
  Serial.printf("Origen: %s\n", source);
  Serial.printf("MQTT: %s\n", sent ? "publicado" : "NO publicado (sin conexion)");
  Serial.println("JSON:");
  Serial.println(payload);
  Serial.println("============================");
}
