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
  // Una sola linea por medicion (a 500 ms un bloque multilinea es ilegible).
  // "\r\n" evita el efecto escalera en terminales que no hacen CR en cada LF.
  Serial.printf("[%s] T=%.2fC H=%.2f%% LUZ=%d MOV=%s %s (%s)\r\n",
                sent ? "MQTT OK " : "MQTT ERR", d.temperature, d.humidity,
                d.lightRaw, d.motion ? "SI" : "NO", status, source);
#if LOG_JSON
  Serial.printf("       %s\r\n", payload);
#endif
}
