#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

struct ColorTheme {
  const char* name;
  // Гармоничный 3-ступенчатый градиент матрицы + родственный акцент + белый (только для искр)
  CRGB ledDeep;
  CRGB ledMain;
  CRGB ledHi;
  CRGB ledAccent;
  CRGB ledWhite;
  // Лаконичный интерфейс (1 основной + 1 акцент + 1 алерт)
  uint16_t uiBg;
  uint16_t uiGrid;
  uint16_t uiDim;
  uint16_t uiMain;
  uint16_t uiAccent;
  uint16_t uiAlert;
};

constexpr int NUM_STATIONS = 10;
inline const char* const WAVE_NAMES[NUM_STATIONS] = {
  "1.LOFI CHILL",
  "2.SYNTHWAVE",
  "3.SPACE TRANCE",
  "4.DUB TECHNO",
  "5.STALKER POST",
  "6.AMBIENT DRONE",
  "7.BLADE RUNNER",
  "8.CYBERPUNK",
  "9.SOLARIS PIANO",
  "10.DEEP HOUSE"
};

constexpr int NUM_DSP = 7;
inline const char* const DSP_NAMES[NUM_DSP] = {
  "CLEAN",
  "BUNKER",
  "TAPE",
  "PHASER",
  "TREMOLO",
  "SPACE",
  "SWEEP"
};

constexpr int NUM_FX = 8;
inline const char* const FX_NAMES[NUM_FX] = {
  "LAVA",
  "BREATHE",
  "BLOOM",
  "TIDE",
  "SECTIONS",
  "HORIZON",
  "RIPPLE",
  "TWILIGHT"
};

constexpr int GRAPH_W = 140;

struct SystemState {
  // Аудио
  uint8_t volPct        = START_VOL_PCT;
  uint8_t dspMode       = 0;
  bool    muted         = false;
  int     stationIdx    = 0;
  volatile bool reseedRequested = true;
  uint16_t currentSeedId   = 101;
  uint32_t lastWaveTouchMs = 0;

  // Фокус энкодеров
  uint8_t enc1Focus = 0;  // 0 = VOL, 1 = WAV, 2 = DSP
  uint8_t enc2Focus = 0;  // 0 = FX,  1 = COL, 2 = BRI

  // Свет и тема
  uint8_t fxMode    = 0;                 // 0..7
  uint8_t colorIdx  = 0;                 // 0..5
  uint8_t brightPct = START_BRIGHT_PCT;  // 0..100%
  bool    lightOff  = false;

  // Тумблер: 0 = CAT, 1 = CLEAN, 2 = SYNC
  uint8_t mode = 1;

  // Сеть
  bool   apConfigMode = false;
  String activeSsid   = "OFFLINE";

  // Телеметрия + время с ПК
  int      cpuLoad      = 0;
  int      ramPct       = 0;
  float    swapGb       = 0.0f;
  float    diskIoMbs    = 0.0f;
  float    netKbs       = 0.0f;
  int      cpuTemp      = 0;
  int      pingMs       = 0;
  bool     swapAlert    = false;
  bool     thermalAlert = false;
  bool     netAlert     = false;
  char     topHog[18]   = "WAITING...";
  char     osType[6]    = "PC";
  char     pcTime[10]   = "";
  char     pcDate[12]   = "";
  uint32_t lastTelemetryMs = 0;
};

extern SystemState state;
extern uint8_t histCpu[GRAPH_W];
extern uint8_t histRam[GRAPH_W];
extern uint8_t histDisk[GRAPH_W];
extern uint8_t histNet[GRAPH_W];

inline bool isPcOnline() {
  return (state.lastTelemetryMs > 0) && (millis() - state.lastTelemetryMs < 5000);
}