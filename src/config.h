#pragma once
#include <Arduino.h>

// ================= WI-FI И СЕТЬ =================
constexpr const char* DEBUG_WIFI_SSID = "";
constexpr const char* DEBUG_WIFI_PASS = "";
constexpr const char* AP_SSID         = "AETHER-MONOLITH";
constexpr const char* MDNS_HOST       = "aether-monolith";
constexpr uint16_t    UDP_PORT        = 4210;

// ================= ЭКРАН ST7789 (170x320, ROTATION 3 = 180°) =================
constexpr uint8_t PIN_TFT_CS  = 15;
constexpr uint8_t PIN_TFT_BLK = 32;

constexpr int UI_X0 = 6;
constexpr int UI_Y0 = 3;
constexpr int UI_W  = 320 - UI_X0 * 2;  // 308 px
constexpr int UI_H  = 170 - UI_Y0 * 2;  // 164 px

// ================= УСИЛИТЕЛЬ MAX98357A (I2S_STD / ESP-IDF 5) =================
constexpr uint8_t  I2S_BCLK        = 26;
constexpr uint8_t  I2S_LRC         = 25;
constexpr uint8_t  I2S_DOUT        = 22;
constexpr uint32_t SAMPLE_RATE     = 22050;
constexpr int      AUDIO_BLOCK_LEN = 256;

// Ограничение максимальной громкости ровно на 40% от прежнего максимума
constexpr float   MAX_AUDIO_GAIN = 0.74f;
constexpr uint8_t START_VOL_PCT  = 25;

// ================= МАТРИЦА WS2812B 8x32 (П-АРКА SCOPE-01) =================
constexpr uint8_t  LED_PIN          = 13;
constexpr uint8_t  MATRIX_W         = 32;
constexpr uint8_t  MATRIX_H         = 8;
constexpr uint16_t NUM_LEDS         = MATRIX_W * MATRIX_H;
constexpr uint8_t  START_BRIGHT_PCT = 60;
constexpr uint32_t MAX_CURRENT_MA   = 480;

// ================= ЭНКОДЕРЫ И 3-ПОЗИЦИОННЫЙ ТУМБЛЕР =================
constexpr uint8_t ENC1_A  = 19;
constexpr uint8_t ENC1_B  = 21;
constexpr uint8_t ENC1_SW = 5;

constexpr uint8_t ENC2_A  = 16;
constexpr uint8_t ENC2_B  = 17;
constexpr uint8_t ENC2_SW = 27;

// Направление вращения энкодеров (-1 исправляет инверсию; при смене проводов поменять на 1)
constexpr int8_t ENC_DIR = -1;

constexpr uint8_t SW_POS_ANIM = 14;  // 1a: Режим кота [CAT]
constexpr uint8_t SW_POS_SYNC = 33;  // 1b: Режим [SYNC]