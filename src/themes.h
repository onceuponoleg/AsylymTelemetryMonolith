#pragma once
#include "types.h"

constexpr int NUM_COLORS = 6;
extern const ColorTheme THEMES[NUM_COLORS];

const ColorTheme& activeTheme();
CRGB sampleThemeLed(const ColorTheme& th, uint8_t intensity, uint8_t accentMix = 0, uint8_t whiteMix = 0);