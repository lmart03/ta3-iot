#pragma once

struct SensorData {
  float temperature;
  float humidity;
  int lightRaw;
  bool motion;
  bool valid;
};
