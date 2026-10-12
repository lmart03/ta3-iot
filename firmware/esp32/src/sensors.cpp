#include "sensors.h"

#include <Arduino.h>
#include <DHTesp.h>
#include <math.h>

#include "config.h"
#include "demo_scenario.h"

namespace {

DHTesp dht;

SensorData readSensors() {
  SensorData data{};

  TempAndHumidity reading = dht.getTempAndHumidity();

  data.temperature = reading.temperature;
  data.humidity = reading.humidity;
  data.lightRaw = analogRead(Config::LDR_PIN);
  data.motion = digitalRead(Config::PIR_PIN) == HIGH;

  data.valid = isfinite(data.temperature) && isfinite(data.humidity) &&
               data.humidity >= 0.0f && data.humidity <= 100.0f;

  return data;
}

}

void setupSensors() {
  dht.setup(Config::DHT_PIN, DHTesp::DHT22);
  pinMode(Config::LDR_PIN, INPUT);
  pinMode(Config::PIR_PIN, INPUT);
  analogReadResolution(12);
}

SensorData acquire() {
  return Config::DEMO ? generateDemoData() : readSensors();
}

bool validate(const SensorData& d) {
  return d.valid && isfinite(d.temperature) && isfinite(d.humidity) &&
         d.temperature > -40.0f && d.temperature < 80.0f &&
         d.humidity >= 0.0f && d.humidity <= 100.0f &&
         d.lightRaw >= 0 && d.lightRaw <= 4095;
}

const char* temperatureStatus(float temp) {
  if (temp >= Config::CRITICAL_TEMP) return "CRITICAL";
  if (temp >= Config::WARNING_TEMP) return "WARNING";
  return "NORMAL";
}
