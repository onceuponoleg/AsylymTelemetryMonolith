#pragma once
#include <Arduino.h>

void audioInitHardware();
void audioPlayTestTone(float freqHz, uint32_t durationMs);
void audioStartTask();