
#include <Arduino.h>
#include <DHTesp.h>
#include <math.h>

// ========================================
// SMART ENVIRONMENT IoT
// Sistema de Monitoreo Ambiental - TA3
// ========================================

namespace Config {
  constexpr char DEVICE_ID[] = "ESP32-001";

  constexpr uint8_t DHT_PIN = 15;
  constexpr uint8_t LDR_PIN = 34;
  constexpr uint8_t PIR_PIN = 27;

  // Una lectura cada 5 segundos
  constexpr unsigned long INTERVAL_MS = 5000;

  // Escenario completo: 5 minutos
  constexpr unsigned long CYCLE_MS = 300000;

  // true = datos sinteticos automaticos
  // false = leer sensores de Wokwi
  constexpr bool DEMO_MODE = true;

  constexpr float WARNING_TEMP = 30.0f;
  constexpr float CRITICAL_TEMP = 35.0f;
}

DHTesp dht;

unsigned long lastRead = 0;
uint32_t sequence = 0;

// ========================================
// ESTRUCTURA DE MEDICIONES
// ========================================

struct SensorData {
  float temperature;
  float humidity;
w  int lightRaw;
  bool motion;
  bool valid;
};

// ========================================
// FUNCIONES AUXILIARES
// ========================================

float interpolate(
  float start,
  float end,
  float fraction
) {
  return start + (end - start) * fraction;
}

// Interpolacion suave sin saltos bruscos
float smoothStep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float transition(
  float elapsed,
  float startTime,
  float endTime,
  float startValue,
  float endValue
) {
  float fraction =
    (elapsed - startTime) /
    (endTime - startTime);

  return interpolate(
    startValue,
    endValue,
    smoothStep(fraction)
  );
}

const char* temperatureStatus(float temp) {
  if (temp >= Config::CRITICAL_TEMP) {
    return "CRITICAL";
  }

  if (temp >= Config::WARNING_TEMP) {
    return "WARNING";
  }

  return "NORMAL";
}


SensorData readSensors() {
  SensorData data{};

  TempAndHumidity reading =
    dht.getTempAndHumidity();

  data.temperature = reading.temperature;
  data.humidity = reading.humidity;
  data.lightRaw = analogRead(Config::LDR_PIN);
  data.motion =
    digitalRead(Config::PIR_PIN) == HIGH;

  data.valid =
    isfinite(data.temperature) &&
    isfinite(data.humidity) &&
    data.humidity >= 0.0f &&
    data.humidity <= 100.0f;

  return data;
}

SensorData generateDemoData() {
  SensorData data{};

  float seconds =
    (millis() % Config::CYCLE_MS) / 1000.0f;

  float baseTemp;

  if (seconds < 50) {
    baseTemp = transition(
      seconds, 0, 50, 24.0f, 25.0f
    );
  }
  else if (seconds < 145) {
    baseTemp = transition(
      seconds, 50, 145, 25.0f, 38.0f
    );
  }
  else if (seconds < 185) {
    baseTemp = transition(
      seconds, 145, 185, 38.0f, 36.5f
    );
  }
  else if (seconds < 265) {
    baseTemp = transition(
      seconds, 185, 265, 36.5f, 24.5f
    );
  }
  else {
    baseTemp = transition(
      seconds, 265, 300, 24.5f, 24.0f
    );
  }

  float tempNoise =
    0.22f * sin(seconds * 0.19f) +
    0.12f * sin(seconds * 0.47f);

  float envelope = min(
    1.0f,
    min(seconds, 300.0f - seconds) / 12.0f
  );

  data.temperature =
    baseTemp + tempNoise * envelope;

  float baseHumidity =
    63.0f - (baseTemp - 24.0f) * 1.45f;

  float humidityNoise =
    1.2f * sin(seconds * 0.11f) +
    0.6f * sin(seconds * 0.27f);

  data.humidity = constrain(
    baseHumidity + humidityNoise * envelope,
    25.0f,
    85.0f
  );

  float light;

  if (seconds < 65) {
    light = transition(
      seconds, 0, 65, 2100, 2700
    );
  }
  else if (seconds < 130) {
    light = transition(
      seconds, 65, 130, 2700, 750
    );
  }
  else if (seconds < 210) {
    light = transition(
      seconds, 130, 210, 750, 450
    );
  }
  else if (seconds < 275) {
    light = transition(
      seconds, 210, 275, 450, 2100
    );
  }
  else {
    light = 2100;
  }

  float lightNoise =
    45.0f * sin(seconds * 0.29f) +
    20.0f * sin(seconds * 0.71f);

  data.lightRaw = constrain(
    (int)(light + lightNoise * envelope),
    0,
    4095
  );

  data.motion =
    (seconds >= 65 && seconds < 85) ||
    (seconds >= 145 && seconds < 160) ||
    (seconds >= 225 && seconds < 245);

  data.valid = true;

  return data;
}

void publishMeasurement() {
  SensorData data = Config::DEMO_MODE
    ? generateDemoData()
    : readSensors();

  if (!data.valid) {
    Serial.println("[ERROR] Lectura invalida");
    return;
  }

  sequence++;

  const char* status =
    temperatureStatus(data.temperature);

  const char* source = Config::DEMO_MODE
    ? "synthetic"
    : "sensor";

  char payload[384];

  int length = snprintf(
    payload,
    sizeof(payload),
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
    Config::DEVICE_ID,
    data.temperature,
    data.humidity,
    data.lightRaw,
    data.motion ? "true" : "false",
    status,
    source,
    (unsigned long)sequence,
    millis()
  );

  if (length < 0 ||
      length >= (int)sizeof(payload)) {
    Serial.println("[ERROR] JSON truncado");
    return;
  }

  Serial.println();
  Serial.println("===== SMART ENVIRONMENT =====");

  Serial.printf(
    "Temperatura: %.2f C\n",
    data.temperature
  );

  Serial.printf(
    "Humedad: %.2f %%\n",
    data.humidity
  );

  Serial.printf(
    "Iluminacion ADC: %d\n",
    data.lightRaw
  );

  Serial.printf(
    "Movimiento: %s\n",
    data.motion ? "DETECTADO" : "NO"
  );

  Serial.printf(
    "Estado: %s\n",
    status
  );

  Serial.printf(
    "Origen: %s\n",
    source
  );

  Serial.println("JSON:");
  Serial.println(payload);

  Serial.println("============================");
}


void setup() {
  Serial.begin(115200);

  dht.setup(Config::DHT_PIN, DHTesp::DHT22);

  pinMode(Config::LDR_PIN, INPUT);
  pinMode(Config::PIR_PIN, INPUT);

  analogReadResolution(12);

  Serial.println();
  Serial.println("================================");
  Serial.println("    SMART ENVIRONMENT IoT");
  Serial.println("================================");

  Serial.printf(
    "Dispositivo: %s\n",
    Config::DEVICE_ID
  );

  Serial.printf(
    "Modo: %s\n",
    Config::DEMO_MODE
      ? "SIMULACION AUTOMATICA"
      : "SENSORES WOKWI"
  );

  Serial.println("Intervalo: 5 segundos");
  Serial.println("Ciclo demo: 5 minutos");
  Serial.println("================================");

  delay(2000);

  publishMeasurement();
  lastRead = millis();
}


void loop() {
  unsigned long now = millis();

  if (now - lastRead >= Config::INTERVAL_MS) {
    lastRead += Config::INTERVAL_MS;
    publishMeasurement();
  }
}
