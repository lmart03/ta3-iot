#pragma once
#include <Arduino.h>

#include "sensor_data.h"

int serializePayload(char* out, size_t size, const SensorData& d,
                     const char* status, const char* source, uint32_t seq);

void printMeasurement(const SensorData& d, const char* status,
                      const char* source, const char* payload, bool sent);
