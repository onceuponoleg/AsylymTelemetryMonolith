#include "themes.h"

// Гармоничные аналогичные палитры для матового диффузора (без грязи на стыках)
const ColorTheme THEMES[NUM_COLORS] = {
  // 0. AMBER: Глубокий кармин -> сочный янтарь -> тёплое золото (акцент: глубокий рубин)
  {
    "AMBER",
    CRGB(92, 8, 0),     CRGB(255, 62, 0),   CRGB(255, 148, 8),
    CRGB(210, 0, 48),   CRGB(255, 232, 198),
    rgb565(0, 0, 0),    rgb565(56, 36, 0),  rgb565(128, 72, 0),
    rgb565(255, 168, 0), rgb565(72, 200, 255), rgb565(255, 36, 0)
  },
  // 1. CYAN: Глубокий индиго -> сочный циан -> аква-мята (акцент: королевский синий)
  {
    "CYAN",
    CRGB(0, 20, 78),    CRGB(0, 175, 255),  CRGB(18, 255, 210),
    CRGB(48, 42, 255),  CRGB(218, 248, 255),
    rgb565(0, 0, 0),    rgb565(0, 44, 88),  rgb565(0, 108, 140),
    rgb565(0, 224, 255), rgb565(255, 148, 0), rgb565(255, 36, 0)
  },
  // 2. EMERALD: Глубокая хвоя -> изумруд -> мягкий лайм (акцент: морская бирюза)
  {
    "EMERALD",
    CRGB(0, 54, 22),    CRGB(0, 225, 64),   CRGB(105, 255, 28),
    CRGB(0, 205, 180),  CRGB(226, 255, 232),
    rgb565(0, 0, 0),    rgb565(0, 48, 0),   rgb565(0, 112, 40),
    rgb565(0, 236, 96), rgb565(255, 76, 224), rgb565(255, 36, 0)
  },
  // 3. VIOLET: Полуночный индиго -> фиолетовый -> орхидея/маджента (акцент: глубокий синий)
  {
    "VIOLET",
    CRGB(26, 0, 82),    CRGB(148, 12, 255), CRGB(242, 22, 185),
    CRGB(24, 78, 255),  CRGB(242, 226, 255),
    rgb565(0, 0, 0),    rgb565(48, 0, 96),  rgb565(112, 44, 152),
    rgb565(196, 96, 255), rgb565(255, 212, 32), rgb565(255, 36, 0)
  },
  // 4. CRIMSON: Глубокое вино -> насыщенный рубин -> тёплый коралл (акцент: глубокий пурпур)
  {
    "CRIMSON",
    CRGB(74, 0, 12),    CRGB(245, 8, 24),   CRGB(255, 76, 10),
    CRGB(185, 0, 92),   CRGB(255, 224, 226),
    rgb565(0, 0, 0),    rgb565(64, 0, 0),   rgb565(128, 24, 32),
    rgb565(255, 52, 72), rgb565(0, 240, 208), rgb565(255, 236, 0)
  },
  // 5. OCEAN: Глубокая бездна -> кобальт -> ледяной голубой (акцент: фиолетовый индиго)
  {
    "OCEAN",
    CRGB(4, 12, 78),    CRGB(18, 78, 255),  CRGB(0, 195, 255),
    CRGB(92, 24, 255),  CRGB(222, 238, 255),
    rgb565(0, 0, 0),    rgb565(0, 36, 96),  rgb565(36, 76, 152),
    rgb565(72, 152, 255), rgb565(255, 132, 0), rgb565(255, 36, 0)
  }
};

const ColorTheme& activeTheme() {
  return THEMES[state.colorIdx % NUM_COLORS];
}

// 3-ступенчатый насыщенный градиент без белого вымывания; белый только при явном whiteMix > 0
CRGB sampleThemeLed(const ColorTheme& th, uint8_t intensity, uint8_t accentMix, uint8_t whiteMix) {
  if (intensity < 6) return CRGB::Black;

  CRGB col;
  if (intensity < 90) {
    col = blend(CRGB::Black, th.ledDeep, map(intensity, 6, 89, 14, 255));
  } else if (intensity < 198) {
    col = blend(th.ledDeep, th.ledMain, map(intensity, 90, 197, 0, 255));
  } else {
    col = blend(th.ledMain, th.ledHi, map(intensity, 198, 255, 0, 255));
  }

  if (accentMix > 0) {
    CRGB acc = th.ledAccent;
    acc.nscale8_video(intensity);
    col = blend(col, acc, ease8InOutCubic(accentMix));
  }

  if (whiteMix > 0) {
    CRGB w = th.ledWhite;
    w.nscale8_video(max(intensity, (uint8_t)175));
    col = blend(col, w, whiteMix);
  }

  return col;
}