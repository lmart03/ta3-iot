#pragma once
#include "sensor_data.h"

void setupSensors();
SensorData acquire();  // sintetico o sensores Wokwi segun DEMO_MODE
bool validate(const SensorData& d);
const char* temperatureStatus(float temp);
