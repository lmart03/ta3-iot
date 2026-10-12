#include "demo_scenario.h"

#include <Arduino.h>
#include <math.h>

#include "config.h"

namespace {

float interpolate(float start, float end, float fraction) {
  return start + (end - start) * fraction;
}

// Interpolacion suave sin saltos bruscos
float smoothStep(float t) {
  t = constrain(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float transition(float elapsed, float startTime, float endTime,
                 float startValue, float endValue) {
  float fraction = (elapsed - startTime) / (endTime - startTime);
  return interpolate(startValue, endValue, smoothStep(fraction));
}

}  // namespace

SensorData generateDemoData() {
  SensorData data{};

  float seconds = (millis() % Config::CYCLE_MS) / 1000.0f;

  // TEMPERATURA: estable -> calentamiento -> critica -> enfriamiento
  float baseTemp;

  if (seconds < 50) {
    baseTemp = transition(seconds, 0, 50, 24.0f, 25.0f);
  } else if (seconds < 145) {
    baseTemp = transition(seconds, 50, 145, 25.0f, 38.0f);
  } else if (seconds < 185) {
    baseTemp = transition(seconds, 145, 185, 38.0f, 36.5f);
  } else if (seconds < 265) {
    baseTemp = transition(seconds, 185, 265, 36.5f, 24.5f);
  } else {
    baseTemp = transition(seconds, 265, 300, 24.5f, 24.0f);
  }

  // Oscilaciones naturales deterministas
  float tempNoise = 0.22f * sin(seconds * 0.19f) + 0.12f * sin(seconds * 0.47f);

  // La amplitud disminuye al cerrar el ciclo
  float envelope = min(1.0f, min(seconds, 300.0f - seconds) / 12.0f);

  data.temperature = baseTemp + tempNoise * envelope;

  // HUMEDAD: baja al aumentar la temperatura + oscilaciones lentas
  float baseHumidity = 63.0f - (baseTemp - 24.0f) * 1.45f;
  float humidityNoise = 1.2f * sin(seconds * 0.11f) + 0.6f * sin(seconds * 0.27f);

  data.humidity =
      constrain(baseHumidity + humidityNoise * envelope, 25.0f, 85.0f);

  // ILUMINACION: valor ADC (0-4095), no lux calibrados
  float light;

  if (seconds < 65) {
    light = transition(seconds, 0, 65, 2100, 2700);
  } else if (seconds < 130) {
    light = transition(seconds, 65, 130, 2700, 750);
  } else if (seconds < 210) {
    light = transition(seconds, 130, 210, 750, 450);
  } else if (seconds < 275) {
    light = transition(seconds, 210, 275, 450, 2100);
  } else {
    light = 2100;
  }

  float lightNoise = 45.0f * sin(seconds * 0.29f) + 20.0f * sin(seconds * 0.71f);

  data.lightRaw = constrain((int)(light + lightNoise * envelope), 0, 4095);

  // MOVIMIENTO: episodios controlados de presencia
  data.motion = (seconds >= 65 && seconds < 85) ||
                (seconds >= 145 && seconds < 160) ||
                (seconds >= 225 && seconds < 245);

  data.valid = true;
  return data;
}
