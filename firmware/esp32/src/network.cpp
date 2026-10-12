#include "network.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "config.h"

namespace {

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastWifiAttempt = 0;
unsigned long lastMqttAttempt = 0;
bool wifiReported = false;
char clientId[40];

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (lastWifiAttempt != 0 && now - lastWifiAttempt < Config::WIFI_RETRY_MS) return;
  lastWifiAttempt = now ? now : 1;

  Serial.printf("[WIFI] Conectando a %s ...\r\n", WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, Config::WIFI_CHANNEL);
}

void maintainMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;

  unsigned long now = millis();
  if (lastMqttAttempt != 0 && now - lastMqttAttempt < Config::MQTT_RETRY_MS) return;
  lastMqttAttempt = now ? now : 1;

  Serial.printf("[MQTT] Conectando a %s:%d (clientId=%s) ...\r\n", MQTT_HOST,
                MQTT_PORT, clientId);

  // Last Will: el broker publica "offline" (retenido) si la conexion se pierde
  const char* lwt = "{\"device_id\":\"ESP32-001\",\"state\":\"offline\"}";
  bool ok = (MQTT_USER[0] != '\0')
                ? mqtt.connect(clientId, MQTT_USER, MQTT_PASSWORD,
                               Config::TOPIC_STATUS, 0, true, lwt)
                : mqtt.connect(clientId, nullptr, nullptr, Config::TOPIC_STATUS,
                               0, true, lwt);

  if (ok) {
    Serial.println("[MQTT] Conectado (confirmado por el broker)");
    mqtt.publish(Config::TOPIC_STATUS,
                 "{\"device_id\":\"ESP32-001\",\"state\":\"online\"}", true);
  } else {
    Serial.printf("[MQTT] Fallo, rc=%d (reintento en %lu s)\r\n", mqtt.state(),
                  Config::MQTT_RETRY_MS / 1000);
  }
}

}  // namespace

void setupNetwork() {
  // client ID unico: base + sufijo derivado de la MAC
  snprintf(clientId, sizeof(clientId), "%s-%08lx", Config::DEVICE_ID,
           (unsigned long)(ESP.getEfuseMac() & 0xFFFFFFFF));

  WiFi.mode(WIFI_STA);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(3);
}

void maintainNetwork() {
  maintainWifi();

  bool wifiUp = WiFi.status() == WL_CONNECTED;
  if (wifiUp && !wifiReported) {
    Serial.printf("[WIFI] Conectado, IP=%s\r\n", WiFi.localIP().toString().c_str());
  } else if (!wifiUp && wifiReported) {
    Serial.println("[WIFI] Conexion perdida");
  }
  wifiReported = wifiUp;

  maintainMqtt();
  mqtt.loop();
}

bool publishTelemetry(const char* payload) {
  return mqtt.connected() && mqtt.publish(Config::TOPIC_TELEMETRY, payload);
}
