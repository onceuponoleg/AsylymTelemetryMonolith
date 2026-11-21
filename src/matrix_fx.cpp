#include "matrix_fx.h"
#include "config.h"
#include "types.h"
#include "themes.h"
#include <FastLED.h>
#include <math.h>

static CRGB leds[NUM_LEDS];

static inline uint16_t xy(uint8_t x, uint8_t y) {
  x = x % MATRIX_W;
  y = constrain(y, 0, MATRIX_H - 1);
  return (x & 0x01) ? ((x * MATRIX_H) + (MATRIX_H - 1 - y)) : ((x * MATRIX_H) + y);
}

void matrixInit() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(75);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_CURRENT_MA);
  FastLED.clear(true);

  const ColorTheme& th = activeTheme();
  for (int i = 0; i < 32; i++) leds[i] = th.ledDeep;
  FastLED.show();
}

void matrixBootSweep(uint32_t elapsedMs) {
  const ColorTheme& th = activeTheme();
  FastLED.setBrightness(85);
  fadeToBlackBy(leds, NUM_LEDS, 36);
  uint8_t baseX = (elapsedMs / 24) % MATRIX_W;
  for (uint8_t y = 0; y < MATRIX_H; y++) {
    leds[xy(baseX, y)] = th.ledMain;
  }
  FastLED.show();
}

void matrixClear() {
  FastLED.clear(true);
}

// Медленные мягкие расцветания для режима BLOOM
struct SoftBloom {
  float cx;
  float cy;
  float age;      // 0..1
  float rate;     // единиц/сек
  float radius;
  bool  accentTint;
  bool  active;
  float waitSec;
};

// Редкие медленно тлеющие звёздочки для режима TWILIGHT
struct StarGlint {
  uint8_t x;
  uint8_t y;
  float   age;    // 0..1
  float   rate;
  bool    isWhite;
  bool    active;
  float   waitSec;
};

static SoftBloom blooms[3] = {};
static StarGlint glints[6] = {};
static bool fxStatesInited = false;

void matrixRender() {
  if (state.lightOff || state.brightPct == 0) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  static uint8_t  lastFx       = 255;
  static uint32_t lastFrameMs  = 0;
  static float    animTime     = 0.0f;   // монотонный интегратор времени (без скачков в SYNC!)
  static float    smoothCpu    = 0.0f;
  static float    smoothMaxBri = 120.0f;

  uint32_t now = millis();
  float dt = (lastFrameMs == 0) ? 0.03f : min(0.10f, (now - lastFrameMs) * 0.001f);
  lastFrameMs = now;

  if (lastFx != state.fxMode) {
    FastLED.clear();
    lastFx = state.fxMode;
  }

  // Плавное сглаживание скорости в SYNC — анимация никогда не дёргается и не прыгает назад
  float targetCpu = (state.mode == 2 && isPcOnline()) ? (float)constrain(state.cpuLoad, 0, 100) : 0.0f;
  smoothCpu += (targetCpu - smoothCpu) * min(1.0f, dt * 1.8f);
  float speedMul = (state.mode == 2) ? (0.85f + (smoothCpu / 100.0f) * 1.15f) : 1.0f;

  float dtAnim = dt * speedMul;
  animTime += dtAnim;

  if (!fxStatesInited) {
    fxStatesInited = true;
    for (int i = 0; i < 3; i++) {
      blooms[i].active  = false;
      blooms[i].waitSec = i * 1.6f;
    }
    for (int i = 0; i < 6; i++) {
      glints[i].active  = false;
      glints[i].waitSec = i * 0.7f;
    }
  }

  const ColorTheme& th = activeTheme();

  switch (state.fxMode % NUM_FX) {
    // ------------------------------------------------------------------------
    // 0. LAVA: Классическая сочная лава (100% гармоничный 3-ступенчатый градиент, без белого мыла)
    // ------------------------------------------------------------------------
    case 0: {
      uint32_t tFlow  = (uint32_t)(animTime * 54.0f);
      uint32_t tShift = (uint32_t)(animTime * 42.0f);
      for (uint8_t x = 0; x < MATRIX_W; x++) {
        for (uint8_t y = 0; y < MATRIX_H; y++) {
          uint8_t raw  = inoise8(x * 22, y * 34 - tFlow, tShift);
          uint8_t blob = scale8(qsub8(raw, 66), 255);
          uint8_t sec  = inoise8(x * 18 + 80, y * 28 + (tFlow >> 1), tShift >> 1);
          uint8_t acc  = (sec > 185) ? map(sec, 185, 245, 0, 150) : 0;
          leds[xy(x, y)] = sampleThemeLed(th, blob, acc, 0);
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 1. BREATHE: Глубокое медитативное дыхание всей арки (подъём к своду и мягкий спад)
    // ------------------------------------------------------------------------
    case 1: {
      // Цикл ~10 секунд
      float inhale = 0.5f - 0.5f * cosf(animTime * 0.62f);
      float eased  = inhale * inhale * (3.0f - 2.0f * inhale);
      // Высота наполнения арки от краёв (0..15.5) к макушке
      float reach = 4.5f + eased * 13.0f;

      for (uint8_t x = 0; x < MATRIX_W; x++) {
        float fromBase = min((float)x, (float)(MATRIX_W - 1 - x)); // 0 у пола .. 15.5 на своде
        float fill = constrain(1.15f - fromBase / reach, 0.0f, 1.0f);
        float glow = 0.16f + (0.28f + 0.56f * eased) * (0.45f + 0.55f * fill);

        for (uint8_t y = 0; y < MATRIX_H; y++) {
          float depthSoft = 0.92f + 0.08f * cosf((y - 3.5f) * 0.45f);
          uint8_t v = (uint8_t)constrain(glow * depthSoft * 255.0f, 18.0f, 255.0f);
          uint8_t acc = (fromBase > 10.0f) ? (uint8_t)(eased * 95.0f) : 0;
          leds[xy(x, y)] = sampleThemeLed(th, v, acc, 0);
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 2. BLOOM: Тёмная спокойная арка + редкие мягкие вспышки-расцветания с белым ядром
    // ------------------------------------------------------------------------
    case 2: {
      for (int i = 0; i < 3; i++) {
        if (!blooms[i].active) {
          blooms[i].waitSec -= dtAnim;
          if (blooms[i].waitSec <= 0.0f) {
            blooms[i].cx         = (float)random8(2, MATRIX_W - 2);
            blooms[i].cy         = (float)random8(1, MATRIX_H - 1);
            blooms[i].age        = 0.0f;
            blooms[i].rate       = 0.15f + (float)random8(0, 8) * 0.012f; // 4..6.5 сек жизни
            blooms[i].radius     = 5.2f + (float)random8(0, 30) * 0.1f;
            blooms[i].accentTint = (i == 2);
            blooms[i].active     = true;
          }
        } else {
          blooms[i].age += dtAnim * blooms[i].rate;
          if (blooms[i].age >= 1.0f) {
            blooms[i].active  = false;
            blooms[i].waitSec = 1.0f + (float)random8(0, 26) * 0.1f;
          }
        }
      }

      for (uint8_t x = 0; x < MATRIX_W; x++) {
        for (uint8_t y = 0; y < MATRIX_H; y++) {
          float accV = 0.11f; // ровный глубокий бархатный фон
          float accA = 0.0f;
          float accW = 0.0f;

          for (int i = 0; i < 3; i++) {
            if (!blooms[i].active) continue;
            float env = powf(sinf(blooms[i].age * PI), 1.5f);
            float dx  = (float)x - blooms[i].cx;
            float dy  = ((float)y - blooms[i].cy) * 1.45f;
            float d   = sqrtf(dx * dx + dy * dy);
            float r   = blooms[i].radius * (0.45f + 0.65f * blooms[i].age);
            if (d < r) {
              float fall = 1.0f - d / r;
              float g    = fall * fall * env;
              accV += g * 0.95f;
              if (blooms[i].accentTint) accA += g * 0.85f;
              // Точечный белый акцент строго в самом центре вспышки на пике
              if (fall > 0.72f) accW += ((fall - 0.72f) / 0.28f) * env * 0.78f;
            }
          }

          uint8_t v = (uint8_t)constrain(accV * 255.0f, 0.0f, 255.0f);
          uint8_t a = (uint8_t)constrain(accA * 255.0f, 0.0f, 220.0f);
          uint8_t w = (uint8_t)constrain(accW * 255.0f, 0.0f, 210.0f);
          leds[xy(x, y)] = sampleThemeLed(th, v, a, w);
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 3. TIDE: Одна широкая шёлковая волна, медленно перекатывающаяся слева направо (~14 с)
    // ------------------------------------------------------------------------
    case 3: {
      float center = 15.5f + sinf(animTime * 0.44f) * 13.0f;
      for (uint8_t x = 0; x < MATRIX_W; x++) {
        float d     = fabsf((float)x - center);
        float swell = expf(-(d * d) / 52.0f);
        uint8_t v   = (uint8_t)constrain((0.10f + swell * 0.90f) * 255.0f, 18.0f, 255.0f);
        // Гармоничный акцент мягко обволакивает дальние плечи волны
        uint8_t acc = (d > 5.0f && d < 12.0f)
                        ? (uint8_t)(sinf(((d - 5.0f) / 7.0f) * PI) * 155.0f)
                        : 0;
        CRGB col = sampleThemeLed(th, v, acc, 0);
        for (uint8_t y = 0; y < MATRIX_H; y++) {
          leds[xy(x, y)] = col;
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 4. SECTIONS: Асинхронное дыхание 3 физических отсеков между обручами (7с / 11с / 15с)
    // ------------------------------------------------------------------------
    case 4: {
      float bayGlow[3];
      bayGlow[0] = 0.5f + 0.5f * sinf(animTime * 0.88f);          // передний отсек (~7 с)
      bayGlow[1] = 0.5f + 0.5f * sinf(animTime * 0.57f + 2.1f);   // средний отсек  (~11 с)
      bayGlow[2] = 0.5f + 0.5f * sinf(animTime * 0.41f + 4.3f);   // задний отсек   (~15 с)

      for (uint8_t y = 0; y < MATRIX_H; y++) {
        uint8_t bay = (y < 3) ? 0 : ((y < 5) ? 1 : 2);
        float g = bayGlow[bay];
        float gSmooth = g * g * (3.0f - 2.0f * g);
        // Каждый отсек чуть смещает мягкий фокус вдоль дуги
        float peakX = 15.5f + sinf(animTime * (0.28f + bay * 0.11f) + bay * 2.0f) * 7.0f;

        for (uint8_t x = 0; x < MATRIX_W; x++) {
          float archFall = expf(-powf((float)x - peakX, 2.0f) / 145.0f);
          float vF = 0.12f + gSmooth * (0.35f + 0.53f * archFall);
          uint8_t v = (uint8_t)constrain(vF * 255.0f, 18.0f, 255.0f);
          uint8_t acc = (bay == 1) ? (uint8_t)(gSmooth * 170.0f) : 0;
          leds[xy(x, y)] = sampleThemeLed(th, v, acc, 0);
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 5. HORIZON: Архитектурный закат — тёплый подъём от обоих бортов к тёмному куполу
    // ------------------------------------------------------------------------
    case 5: {
      float tilt   = sinf(animTime * 0.32f) * 0.28f; // медленный наклон баланса лево <-> право
      float breath = 0.82f + 0.18f * sinf(animTime * 0.52f);

      for (uint8_t x = 0; x < MATRIX_W; x++) {
        float normX = ((float)x - 15.5f) / 15.5f;      // -1 левый низ .. 0 свод .. +1 правый низ
        float flank = fabsf(normX);                     // 1 у обоих бортов, 0 на вершине свода
        float sideBias = constrain(1.0f + normX * tilt, 0.65f, 1.25f);

        float vF = (0.16f + powf(flank, 1.35f) * 0.84f) * sideBias * breath;
        uint8_t v = (uint8_t)constrain(vF * 255.0f, 20.0f, 255.0f);
        // Вершина свода окрашена глубоким родственным акцентом (ночное небо над горизонтом)
        uint8_t acc = (flank < 0.55f) ? (uint8_t)((1.0f - flank / 0.55f) * 200.0f) : 0;
        CRGB col = sampleThemeLed(th, v, acc, 0);
        for (uint8_t y = 0; y < MATRIX_H; y++) {
          leds[xy(x, y)] = col;
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 6. RIPPLE: Ультра-медленные круги на воде, стекающие с вершины свода к краям
    // ------------------------------------------------------------------------
    case 6: {
      float phase = animTime * 1.15f; // ~5.5 с на одну плавную волну
      for (uint8_t x = 0; x < MATRIX_W; x++) {
        float distTop = fabsf((float)x - 15.5f);
        for (uint8_t y = 0; y < MATRIX_H; y++) {
          float dy = fabsf((float)y - 3.5f) * 0.45f;
          float wave = 0.5f + 0.5f * sinf((distTop + dy) * 0.55f - phase);
          float softWave = powf(wave, 1.8f);
          uint8_t v = (uint8_t)constrain((0.12f + softWave * 0.86f) * 255.0f, 18.0f, 255.0f);
          uint8_t acc = (softWave < 0.25f) ? (uint8_t)((0.25f - softWave) * 520.0f) : 0;
          leds[xy(x, y)] = sampleThemeLed(th, v, acc, 0);
        }
      }
      break;
    }

    // ------------------------------------------------------------------------
    // 7. TWILIGHT: Глубокий ночной фон + редкие медленно всплывающие звёздные блики
    // ------------------------------------------------------------------------
    default: {
      for (int i = 0; i < 6; i++) {
        if (!glints[i].active) {
          glints[i].waitSec -= dtAnim;
          if (glints[i].waitSec <= 0.0f) {
            glints[i].x       = random8(1, MATRIX_W - 1);
            glints[i].y       = random8(0, MATRIX_H);
            glints[i].age     = 0.0f;
            glints[i].rate    = 0.22f + (float)random8(0, 12) * 0.015f; // 2.5..4.5 с плавного дыхания
            glints[i].isWhite = ((i % 2) == 0); // половина — мягкий белый блик, половина — акцент
            glints[i].active  = true;
          }
        } else {
          glints[i].age += dtAnim * glints[i].rate;
          if (glints[i].age >= 1.0f) {
            glints[i].active  = false;
            glints[i].waitSec = 0.6f + (float)random8(0, 22) * 0.1f;
          }
        }
      }

      // Спокойный глубокий фон по дуге
      float bgBreath = 0.75f + 0.25f * sinf(animTime * 0.35f);
      for (uint8_t x = 0; x < MATRIX_W; x++) {
        float archV = 0.20f + 0.18f * cosf(((float)x - 15.5f) / 15.5f * 1.4f);
        uint8_t baseV = (uint8_t)(archV * bgBreath * 255.0f);
        CRGB bgCol = sampleThemeLed(th, baseV, 0, 0);
        for (uint8_t y = 0; y < MATRIX_H; y++) {
          leds[xy(x, y)] = bgCol;
        }
      }

      // Накладываем мягкие точечные блики (маленькое гало 1.8 диода)
      for (int i = 0; i < 6; i++) {
        if (!glints[i].active) continue;
        float env = powf(sinf(glints[i].age * PI), 1.6f);
        for (int8_t dx = -1; dx <= 1; dx++) {
          int px = (int)glints[i].x + dx;
          if (px < 0 || px >= MATRIX_W) continue;
          for (int8_t dy = -1; dy <= 1; dy++) {
            int py = (int)glints[i].y + dy;
            if (py < 0 || py >= MATRIX_H) continue;
            float d = sqrtf((float)(dx * dx + dy * dy));
            if (d > 1.5f) continue;
            float fall = (1.0f - d / 1.6f) * env;
            uint8_t v = (uint8_t)constrain(95.0f + fall * 160.0f, 0.0f, 255.0f);
            uint8_t a = glints[i].isWhite ? 0 : (uint8_t)(fall * 210.0f);
            uint8_t w = (glints[i].isWhite && dx == 0 && dy == 0) ? (uint8_t)(env * 215.0f) : 0;
            CRGB starCol = sampleThemeLed(th, v, a, w);
            leds[xy(px, py)] = blend(leds[xy(px, py)], starCol, (uint8_t)constrain(fall * 255.0f, 0.0f, 255.0f));
          }
        }
      }
      break;
    }
  }

  // --- АЛЕРТЫ ПК ПОВЕРХ АНИМАЦИИ ---
  if (state.swapAlert) {
    uint8_t pulse = beatsin8(45, 90, 255);
    for (uint8_t x = 0; x < MATRIX_W; x += 2) leds[xy(x, 0)] = CRGB(pulse, 0, pulse);
  }
  if (state.thermalAlert) {
    uint8_t redPulse = beatsin8(90, 130, 255);
    for (uint8_t y = 0; y < MATRIX_H; y++) {
      for (uint8_t k = 0; k < 3; k++) {
        leds[xy(k, y)]                = CRGB(redPulse, 8, 0);
        leds[xy(MATRIX_W - 1 - k, y)] = CRGB(redPulse, 8, 0);
      }
    }
  }
  if (state.netAlert && random8() < 35) {
    leds[xy(random8(10, 22), random8(0, MATRIX_H))] = CRGB::White;
  }

  // --- РАВНОМЕРНАЯ ЯРКОСТЬ 0..100% С УЧЁТОМ ЛИМИТА ТОКА ---
  uint8_t frameSafeMax = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, 220, 5, MAX_CURRENT_MA);
  if (frameSafeMax < 36) frameSafeMax = 36;
  smoothMaxBri = smoothMaxBri * 0.92f + (float)frameSafeMax * 0.08f;

  float norm = (float)state.brightPct / 100.0f;
  float gammaVal = powf(norm, 1.65f);
  uint8_t finalBri = max(4, (int)roundf(gammaVal * smoothMaxBri));

  FastLED.setBrightness(finalBri);
  FastLED.show();
}