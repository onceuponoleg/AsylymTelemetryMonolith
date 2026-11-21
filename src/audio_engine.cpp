#include "audio_engine.h"
#include "config.h"
#include "types.h"
#include <driver/i2s_std.h>
#include <math.h>

// ================= ТАБЛИЦА СИНУСА И I2S =================
constexpr int SIN_LUT_SIZE = 1024;
static float sinLUT[SIN_LUT_SIZE];
static i2s_chan_handle_t txHandle = NULL;

// Увеличенный кольцевой буфер (~278 мс @ 22050 Гц = 12 КБ) для глубокого SPACE и TAPE
constexpr int DELAY_LEN = 6144;
static int16_t* delayLine = nullptr;
static int delayIdx = 0;

static void buildSineTable() {
  for (int i = 0; i < SIN_LUT_SIZE; i++) {
    sinLUT[i] = sinf((i * TWO_PI) / SIN_LUT_SIZE);
  }
}

static inline float fastSinNorm(float p) {
  p -= (int)p;
  if (p < 0.0f) p += 1.0f;
  float idxF = p * SIN_LUT_SIZE;
  int i0 = (int)idxF;
  float frac = idxF - i0;
  int i1 = (i0 + 1) & (SIN_LUT_SIZE - 1);
  i0 &= (SIN_LUT_SIZE - 1);
  return sinLUT[i0] + frac * (sinLUT[i1] - sinLUT[i0]);
}

static inline float midiToFreq(float midiNote) {
  return 440.0f * powf(2.0f, (midiNote - 69.0f) / 12.0f);
}

// ================= НЕПОВТОРЯЮЩИЙСЯ ПРОЦЕДУРНЫЙ КОМПОЗИТОР (10 СТИЛЕЙ) =================
static const int SCALES[8][8] = {
  {0, 2, 3, 5, 7, 9, 10, 12},  // 0: Dorian (Lofi / Dub / Deep House)
  {0, 2, 3, 5, 7, 8, 10, 12},  // 1: Natural Minor (Synthwave / Space)
  {0, 2, 3, 5, 7, 8, 11, 12},  // 2: Harmonic Minor (Stalker Post)
  {0, 2, 4, 6, 7, 9, 11, 12},  // 3: Lydian (Ambient Drone)
  {0, 2, 3, 5, 7, 9, 10, 14},  // 4: Dorian9 (Blade Runner)
  {0, 1, 3, 5, 7, 8, 10, 12},  // 5: Phrygian (Cyberpunk)
  {0, 3, 5, 7, 10, 12, 15, 17},// 6: Minor Pentatonic (Solaris Piano)
  {0, 2, 4, 7, 9, 11, 12, 14}  // 7: Major 7/9 (Warm Chill)
};

static const int PROGRESSIONS[14][4] = {
  {0, 5, 2, 6}, {0, 3, 5, 4}, {0, 5, 3, 4}, {0, 2, 5, 6},
  {0, 3, 6, 2}, {0, 4, 5, 3}, {0, 6, 5, 4}, {0, 2, 3, 5},
  {5, 3, 0, 4}, {0, 1, 3, 2}, {0, 4, 2, 6}, {3, 5, 0, 4},
  {0, 5, 4, 2}, {2, 6, 0, 5}
};

struct StyleSpec {
  uint8_t  scaleA;
  uint8_t  scaleB;
  uint32_t stepMin;
  uint32_t stepMax;
  uint8_t  densityPct;   // плотность мелодии
  uint8_t  kickMode;     // 0=none, 1=soft, 2=four-on-floor, 3=syncopated
  float    leadDecay;
  float    chordDecay;
  float    delayFb;
};

static const StyleSpec STYLES[NUM_STATIONS] = {
  // 0: LOFI CHILL
  {0, 7, 3600, 4400, 52, 1, 0.9992f, 0.9996f, 0.26f},
  // 1: SYNTHWAVE
  {1, 0, 2850, 3550, 68, 1, 0.9990f, 0.9995f, 0.32f},
  // 2: SPACE TRANCE
  {1, 3, 2250, 2850, 76, 2, 0.9988f, 0.9994f, 0.42f},
  // 3: DUB TECHNO
  {0, 5, 2900, 3450, 38, 2, 0.9984f, 0.9989f, 0.46f},
  // 4: STALKER POST
  {2, 1, 3850, 4700, 46, 0, 0.9994f, 0.9997f, 0.34f},
  // 5: AMBIENT DRONE
  {3, 6, 5200, 6600, 34, 0, 0.99975f, 0.99988f, 0.44f},
  // 6: BLADE RUNNER
  {4, 1, 4100, 5100, 44, 0, 0.9996f, 0.99982f, 0.40f},
  // 7: CYBERPUNK
  {5, 2, 2400, 2950, 72, 3, 0.9985f, 0.9991f, 0.28f},
  // 8: SOLARIS PIANO
  {6, 3, 3900, 4900, 48, 0, 0.9995f, 0.9997f, 0.36f},
  // 9: DEEP HOUSE
  {0, 1, 2750, 3250, 58, 2, 0.9989f, 0.9993f, 0.34f}
};

struct GenerativeSong {
  int      rootMidi = 55;
  int      scale[8];
  int      chordMain[4];
  int      chordBridge[4];
  uint8_t  phraseA[16];
  uint8_t  phraseB[16];
  bool     actA[16];
  bool     actB[16];
  int8_t   octA[16];      // 0 или +12
  int8_t   octB[16];
  uint32_t stepSamples = 3400;
  uint8_t  bassPattern = 0;
  uint8_t  chordVoicing = 0;
};

static GenerativeSong song;
static int lastRootMidi = -1;
static int lastProgIdx  = -1;

static void buildMusicalPhrase(uint8_t notes[16], bool active[16], int8_t oct[16],
                               uint8_t densityPct, int styleIdx) {
  int cursor = esp_random() % 6;
  uint8_t contour = esp_random() % 4; // 0: волна, 1: подъём, 2: спуск, 3: арпеджио

  for (int s = 0; s < 16; s++) {
    uint32_t r = esp_random();

    // Музыкальные шаги по ступеням вместо хаотичных скачков
    int delta = 0;
    if (contour == 3) {
      delta = ((s & 1) == 0) ? 2 : -1;
    } else if (contour == 1) {
      delta = (r % 100 < 65) ? 1 : -1;
    } else if (contour == 2) {
      delta = (r % 100 < 65) ? -1 : 1;
    } else {
      static const int8_t steps[5] = {-2, -1, 0, 1, 2};
      delta = steps[r % 5];
    }
    cursor = constrain(cursor + delta, 0, 7);
    notes[s] = (uint8_t)cursor;

    // Редкие октавные акценты для выразительности
    oct[s] = ((r >> 8) % 100 < 18) ? 12 : 0;

    // Ритмический рисунок зависит от стиля + случайный вариатор
    bool strongBeat = (s == 0 || s == 8);
    bool offBeat    = (s == 3 || s == 6 || s == 10 || s == 14);
    if (styleIdx == 3) {
      active[s] = (s == 0 || s == 6 || s == 10 || ((r >> 12) % 100 < densityPct));
    } else if (styleIdx == 5 || styleIdx == 8) {
      active[s] = (s == 0) || ((r >> 12) % 100 < densityPct);
    } else {
      active[s] = strongBeat || (offBeat && ((r >> 12) % 100 < densityPct + 12)) ||
                  ((r >> 16) % 100 < densityPct);
    }
  }
}

static void generateNewSongSeed(int styleIdx) {
  styleIdx = styleIdx % NUM_STATIONS;
  const StyleSpec& sp = STYLES[styleIdx];

  // Забираем свежую аппаратную энтропию
  uint32_t r = esp_random() ^ (micros() * 2654435761UL);
  state.currentSeedId = (uint16_t)((r & 0x0FFF) + 100);

  // Все 12 тональностей (E3..D#4 = MIDI 52..63), гарантируем смену ключа
  int newRoot = 52 + (r % 12);
  if (newRoot == lastRootMidi) newRoot = 52 + ((newRoot - 52 + 5) % 12);
  lastRootMidi  = newRoot;
  song.rootMidi = newRoot;

  // Выбор лада (основной или альтернативный для данного стиля)
  uint8_t scIdx = ((r >> 5) & 1) ? sp.scaleB : sp.scaleA;
  memcpy(song.scale, SCALES[scIdx], sizeof(song.scale));

  // Гарантируем смену аккордовой прогрессии + отдельный бридж
  int pMain = (r >> 8) % 14;
  if (pMain == lastProgIdx) pMain = (pMain + 3) % 14;
  lastProgIdx = pMain;
  int pBridge = (pMain + 1 + ((r >> 12) % 12)) % 14;

  for (int i = 0; i < 4; i++) {
    song.chordMain[i]   = PROGRESSIONS[pMain][i];
    song.chordBridge[i] = PROGRESSIONS[pBridge][i];
  }

  song.stepSamples  = sp.stepMin + (esp_random() % (sp.stepMax - sp.stepMin + 1));
  song.bassPattern  = esp_random() % 4;
  song.chordVoicing = esp_random() % 3; // 0=triad, 1=add7th, 2=open fifth+9th

  buildMusicalPhrase(song.phraseA, song.actA, song.octA, sp.densityPct, styleIdx);
  buildMusicalPhrase(song.phraseB, song.actB, song.octB, sp.densityPct, styleIdx);
}

// ================= 6 МУЗЫКАЛЬНЫХ DSP-ЭФФЕКТОВ (БЕЗ ШУМОВ) =================
struct DspState {
  // 2-pole SVF (BUNKER и SWEEP)
  float svfLow  = 0.0f;
  float svfBand = 0.0f;

  // 4-каскадный All-Pass PHASER
  float apZ[4] = {0, 0, 0, 0};

  // LFO-фазы
  float lfoPhaser  = 0.0f;
  float lfoTremolo = 0.0f;
  float lfoSweep   = 0.0f;
  float lfoTape1   = 0.0f;
  float lfoTape2   = 0.0f;

  // Фильтры теплоты и демпфирования хвостов
  float tapeLp  = 0.0f;
  float spaceLp = 0.0f;
  float tremLp  = 0.0f;
};

static DspState dsp;

static inline float readDelayInterp(float delaySamples) {
  if (!delayLine) return 0.0f;
  int dInt = (int)delaySamples;
  float frac = delaySamples - dInt;
  int idxA = (delayIdx - dInt + DELAY_LEN) % DELAY_LEN;
  int idxB = (idxA - 1 + DELAY_LEN) % DELAY_LEN;
  return (float)delayLine[idxA] + frac * ((float)delayLine[idxB] - (float)delayLine[idxA]);
}

static inline float processMusicalDsp(float inSig, uint8_t dspMode, float baseDelayFb) {
  float out = inSig;

  switch (dspMode) {
    // 0. CLEAN: Чистый прозрачный звук + деликатный студийный дилей
    case 0: {
      if (delayLine) {
        int tapIdx = (delayIdx - 2800 + DELAY_LEN) % DELAY_LEN;
        float delayed = (float)delayLine[tapIdx];
        out = inSig + delayed * baseDelayFb * 0.75f;
        delayLine[delayIdx] = (int16_t)constrain(inSig + delayed * baseDelayFb * 0.60f, -29000.0f, 29000.0f);
        delayIdx = (delayIdx + 1) % DELAY_LEN;
      }
      return out;
    }

    // 1. BUNKER: Глубокий уютный Low-Pass срез (~540 Гц) — эффект музыки за бетонной стеной
    case 1: {
      float dryWithEcho = inSig;
      if (delayLine) {
        int tapIdx = (delayIdx - 3200 + DELAY_LEN) % DELAY_LEN;
        float del = (float)delayLine[tapIdx];
        dryWithEcho = inSig + del * 0.25f;
        delayLine[delayIdx] = (int16_t)constrain(dryWithEcho * 0.70f, -29000.0f, 29000.0f);
        delayIdx = (delayIdx + 1) % DELAY_LEN;
      }
      // 2-pole SVF LPF: cutoff ~540 Hz (f ≈ 0.153 @ 22050 Hz), q = 0.85
      const float f = 0.155f;
      const float q = 0.85f;
      dsp.svfLow  += f * dsp.svfBand;
      float high   = dryWithEcho - dsp.svfLow - q * dsp.svfBand;
      dsp.svfBand += f * high;
      // Компенсация громкости (+30%), чтобы переход в BUNKER был ровным
      return dsp.svfLow * 1.30f;
    }

    // 2. TAPE: Тёплая аналоговая плёнка (мягкий wow/flutter + тёплый верх + мягкая сатурация)
    case 2: {
      dsp.lfoTape1 += 0.42f / SAMPLE_RATE;
      if (dsp.lfoTape1 >= 1.0f) dsp.lfoTape1 -= 1.0f;
      dsp.lfoTape2 += 2.35f / SAMPLE_RATE;
      if (dsp.lfoTape2 >= 1.0f) dsp.lfoTape2 -= 1.0f;

      float warped = inSig;
      if (delayLine) {
        delayLine[delayIdx] = (int16_t)constrain(inSig, -29000.0f, 29000.0f);
        float dSamp = 140.0f + 20.0f * fastSinNorm(dsp.lfoTape1) + 4.5f * fastSinNorm(dsp.lfoTape2);
        warped = readDelayInterp(dSamp);
        float slap = readDelayInterp(2650.0f);
        warped = warped * 0.88f + slap * 0.24f;
        delayIdx = (delayIdx + 1) % DELAY_LEN;
      }
      // Мягкий срез резких верхов (~2.4 кГц)
      dsp.tapeLp += 0.44f * (warped - dsp.tapeLp);
      // Тёплая кубическая плёночная сатурация (без жёсткого клиппинга)
      float norm = constrain(dsp.tapeLp / 22000.0f, -1.2f, 1.2f);
      float sat  = norm - 0.16f * norm * norm * norm;
      return sat * 24500.0f;
    }

    // 3. PHASER: Плавный 4-каскадный всепропускающий фейзер (переливы даже в моно!)
    case 3: {
      dsp.lfoPhaser += 0.16f / SAMPLE_RATE;
      if (dsp.lfoPhaser >= 1.0f) dsp.lfoPhaser -= 1.0f;

      float a = 0.20f + 0.62f * (0.5f + 0.5f * fastSinNorm(dsp.lfoPhaser));
      float s = inSig;
      for (int st = 0; st < 4; st++) {
        float y = -a * s + dsp.apZ[st];
        dsp.apZ[st] = s + a * y;
        s = y;
      }
      float phased = inSig * 0.62f + s * 0.52f;
      if (delayLine) {
        int tapIdx = (delayIdx - 3100 + DELAY_LEN) % DELAY_LEN;
        float del = (float)delayLine[tapIdx];
        delayLine[delayIdx] = (int16_t)constrain(phased * 0.72f + del * 0.26f, -29000.0f, 29000.0f);
        delayIdx = (delayIdx + 1) % DELAY_LEN;
        phased += del * 0.24f;
      }
      return phased;
    }

    // 4. TREMOLO: Шёлковое оптическое тремоло (глубина 36%, без провалов в ноль)
    case 4: {
      dsp.lfoTremolo += 2.3f / SAMPLE_RATE;
      if (dsp.lfoTremolo >= 1.0f) dsp.lfoTremolo -= 1.0f;

      float mod = 0.76f + 0.24f * fastSinNorm(dsp.lfoTremolo);
      dsp.tremLp += 0.55f * (inSig - dsp.tremLp);
      float wet = dsp.tremLp * mod * 1.12f;

      if (delayLine) {
        int tapIdx = (delayIdx - 3600 + DELAY_LEN) % DELAY_LEN;
        float del = (float)delayLine[tapIdx];
        delayLine[delayIdx] = (int16_t)constrain(wet * 0.75f + del * 0.25f, -29000.0f, 29000.0f);
        delayIdx = (delayIdx + 1) % DELAY_LEN;
        wet += del * 0.25f;
      }
      return wet;
    }

    // 5. SPACE: Глубокий 3-отводный диффузный космо-хвост с тёплым демпфированием
    case 5: {
      if (!delayLine) return inSig;
      float t1 = (float)delayLine[(delayIdx - 1721 + DELAY_LEN) % DELAY_LEN];
      float t2 = (float)delayLine[(delayIdx - 3853 + DELAY_LEN) % DELAY_LEN];
      float t3 = (float)delayLine[(delayIdx - 6089 + DELAY_LEN) % DELAY_LEN];

      float diffuser = t1 * 0.42f + t2 * 0.34f + t3 * 0.24f;
      // Хвосты плавно темнеют с каждым отражением, не превращаясь в кашу
      dsp.spaceLp += 0.34f * (diffuser - dsp.spaceLp);

      float toLine = inSig * 0.72f + dsp.spaceLp * 0.54f;
      delayLine[delayIdx] = (int16_t)constrain(toLine, -29000.0f, 29000.0f);
      delayIdx = (delayIdx + 1) % DELAY_LEN;

      return inSig * 0.78f + dsp.spaceLp * 0.65f;
    }

    // 6. SWEEP: Медленный дыхательный резонансный фильтр (цикл ~7.5 сек — гипнотично и мягко)
    default: {
      dsp.lfoSweep += 0.135f / SAMPLE_RATE;
      if (dsp.lfoSweep >= 1.0f) dsp.lfoSweep -= 1.0f;

      float wave = 0.5f + 0.5f * fastSinNorm(dsp.lfoSweep);
      // Частота среза плавно скользит 380 Гц .. 2200 Гц (f: 0.11 .. 0.61)
      float f = 0.11f + 0.50f * (wave * wave);
      const float q = 0.72f;

      float src = inSig;
      if (delayLine) {
        int tapIdx = (delayIdx - 3400 + DELAY_LEN) % DELAY_LEN;
        float del = (float)delayLine[tapIdx];
        src = inSig + del * 0.28f;
        delayLine[delayIdx] = (int16_t)constrain(src * 0.68f, -29000.0f, 29000.0f);
        delayIdx = (delayIdx + 1) % DELAY_LEN;
      }

      dsp.svfLow  += f * dsp.svfBand;
      float high   = src - dsp.svfLow - q * dsp.svfBand;
      dsp.svfBand += f * high;
      return dsp.svfLow * 1.15f;
    }
  }
}

// ================= ИНИЦИАЛИЗАЦИЯ I2S И ЗАДАЧА ЯДРА 0 =================
void audioInitHardware() {
  buildSineTable();
  if (!delayLine) {
    delayLine = (int16_t*)calloc(DELAY_LEN, sizeof(int16_t));
  }

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num  = 8;
  chan_cfg.dma_frame_num = 512;
  chan_cfg.auto_clear    = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &txHandle, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCLK,
      .ws   = (gpio_num_t)I2S_LRC,
      .dout = (gpio_num_t)I2S_DOUT,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(txHandle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(txHandle));
}

void audioPlayTestTone(float freqHz, uint32_t durationMs) {
  if (!txHandle) return;
  int32_t totalSamples = (SAMPLE_RATE * durationMs) / 1000;
  int16_t buf[AUDIO_BLOCK_LEN * 2];
  float phase = 0.0f;
  float phaseInc = freqHz / SAMPLE_RATE;

  int32_t writtenSamples = 0;
  while (writtenSamples < totalSamples) {
    for (int i = 0; i < AUDIO_BLOCK_LEN; i++) {
      int32_t cur = writtenSamples + i;
      float env = 1.0f;
      if (cur < 400) env = cur / 400.0f;
      else if (cur > totalSamples - 600) env = max(0.0f, (totalSamples - cur) / 600.0f);

      int16_t val = (int16_t)(fastSinNorm(phase) * 1400.0f * env);
      phase += phaseInc;
      if (phase >= 1.0f) phase -= 1.0f;
      buf[i * 2]     = val;
      buf[i * 2 + 1] = val;
    }
    size_t bytesWritten = 0;
    i2s_channel_write(txHandle, buf, sizeof(buf), &bytesWritten, portMAX_DELAY);
    writtenSamples += AUDIO_BLOCK_LEN;
  }
}

static void audioGenerativeTask(void* parameter) {
  int16_t outBuf[AUDIO_BLOCK_LEN * 2];

  float phChord[4]    = {0, 0, 0, 0};
  float freqChord[4]  = {220, 261.6f, 329.6f, 392.0f};
  float phLead        = 0.0f;
  float phLeadDetune  = 0.0f;
  float phBass        = 0.0f;
  float phVib         = 0.0f;

  float targetFreqLead = 440.0f, curFreqLead = 440.0f;
  float freqBass = 110.0f;

  float envLead = 0.0f, envLeadTarget = 0.0f;
  float envChord = 0.0f, envChordTarget = 0.0f;
  float envKick = 0.0f, envKickTarget = 0.0f;

  float dcBlockState = 0.0f;
  float slowMoFactor = 1.0f;
  float smoothVol    = 0.0f;

  uint32_t sampleInStep = 0;
  int currentStep = 0;
  int currentBar  = 0;

  generateNewSongSeed(state.stationIdx);

  for (;;) {
    int style = state.stationIdx % NUM_STATIONS;
    const StyleSpec& sp = STYLES[style];

    if (state.reseedRequested) {
      state.reseedRequested = false;
      generateNewSongSeed(style);
      currentStep  = 0;
      currentBar   = 0;
      sampleInStep = 0;
    }

    bool anyAlert = (state.thermalAlert || state.swapAlert || state.netAlert);
    float targetSlowMo = (state.mode == 2 && anyAlert) ? 0.65f : 1.0f;
    slowMoFactor = 0.95f * slowMoFactor + 0.05f * targetSlowMo;

    uint32_t activeStepLen = (uint32_t)(song.stepSamples / slowMoFactor);

    // Плавный перцептивный расчёт громкости с лимитом 40% от прошлого максимума
    float normVol = state.muted ? 0.0f : ((float)state.volPct / 100.0f);
    float targetGain = (normVol <= 0.0f) ? 0.0f : (powf(normVol, 1.35f) * MAX_AUDIO_GAIN);

    for (int i = 0; i < AUDIO_BLOCK_LEN; i++) {
      smoothVol += 0.002f * (targetGain - smoothVol);
      if (smoothVol < 0.0005f) {
        outBuf[i * 2]     = 0;
        outBuf[i * 2 + 1] = 0;
        continue;
      }

      // --- 1. МАКРО-СЕКВЕНСОР (A -> A' -> B -> BRIDGE) ---
      if (sampleInStep == 0) {
        int barInCycle = currentBar & 3; // 0,1 = A, 2 = B, 3 = Bridge
        bool useBridge = (barInCycle == 3);
        bool usePhraseB = (barInCycle >= 2);

        int chordIdx = (currentStep / 4) % 4;
        int deg = useBridge ? song.chordBridge[chordIdx] : song.chordMain[chordIdx];

        freqChord[0] = midiToFreq(song.rootMidi + song.scale[deg % 7]);
        freqChord[1] = midiToFreq(song.rootMidi + song.scale[(deg + 2) % 7]);
        freqChord[2] = midiToFreq(song.rootMidi + song.scale[(deg + 4) % 7]);
        int topTone  = (song.chordVoicing == 0) ? (deg % 7) : ((deg + 6) % 7);
        freqChord[3] = midiToFreq(song.rootMidi + 12 + song.scale[topTone]);

        int bassShift = (song.bassPattern == 2 && (currentStep % 4 == 2)) ? 4 : 0;
        freqBass = midiToFreq(song.rootMidi + song.scale[(deg + bassShift) % 7] - 12);
        while (freqBass < 82.0f) freqBass *= 2.0f;

        // Аккорды и бочка по характеру стиля
        if (style == 3) {
          if (currentStep % 4 == 2) envChordTarget = 1.0f;
          if (currentStep % 4 == 0) envKickTarget  = 1.0f;
        } else {
          if (currentStep % 4 == 0) envChordTarget = 1.0f;
          if (sp.kickMode == 1 && (currentStep == 0 || currentStep == 8)) envKickTarget = 0.78f;
          else if (sp.kickMode == 2 && (currentStep % 4 == 0))            envKickTarget = 0.88f;
          else if (sp.kickMode == 3 && (currentStep == 0 || currentStep == 6 || currentStep == 10)) envKickTarget = 0.92f;
        }

        bool stepOn = usePhraseB ? song.actB[currentStep] : song.actA[currentStep];
        if (stepOn) {
          uint8_t stepDeg = usePhraseB ? song.phraseB[currentStep] : song.phraseA[currentStep];
          int8_t  octAdd  = usePhraseB ? song.octB[currentStep]    : song.octA[currentStep];
          int melIdx = (deg + stepDeg) % 8;
          targetFreqLead = midiToFreq(song.rootMidi + 12 + song.scale[melIdx] + octAdd);
          envLeadTarget  = 1.0f;
        }
      }

      sampleInStep++;
      if (sampleInStep >= activeStepLen) {
        sampleInStep = 0;
        currentStep = (currentStep + 1) & 15;
        if (currentStep == 0) {
          currentBar++;
          // Живая постепенная эволюция: каждые 4 такта мягко обновляем часть мотива
          if ((currentBar % 4) == 0) {
            int pos1 = esp_random() % 16;
            int pos2 = esp_random() % 16;
            song.phraseA[pos1] = (song.phraseA[pos1] + 1 + (esp_random() % 3)) % 8;
            song.phraseB[pos2] = (song.phraseB[pos2] + 6 - (esp_random() % 3)) % 8;
            if ((esp_random() & 3) == 0) song.actB[pos2] = !song.actB[pos2];
          }
          // Каждые 16 тактов — новый свежий мотив внутри того же стиля
          if ((currentBar % 16) == 0) {
            buildMusicalPhrase(song.phraseB, song.actB, song.octB, sp.densityPct, style);
          }
        }
      }

      // --- 2. ОГИБАЮЩИЕ ---
      float leadAtk  = (style == 5 || style == 6) ? 0.0022f : 0.015f;
      float chordAtk = (style == 5 || style == 6) ? 0.0014f : 0.006f;

      if (envLeadTarget > 0.0f) {
        envLead += leadAtk * (1.05f - envLead);
        if (envLead >= 0.98f) envLeadTarget = 0.0f;
      } else {
        envLead *= sp.leadDecay;
      }

      if (envChordTarget > 0.0f) {
        envChord += chordAtk * (1.05f - envChord);
        if (envChord >= 0.98f) envChordTarget = 0.0f;
      } else {
        envChord *= sp.chordDecay;
      }

      if (envKickTarget > 0.0f) {
        envKick += 0.025f * (1.05f - envKick);
        if (envKick >= 0.96f) envKickTarget = 0.0f;
      } else {
        envKick *= 0.9960f;
      }

      float glide = (style == 6 || style == 5) ? 0.0035f : 0.025f;
      curFreqLead += glide * (targetFreqLead - curFreqLead);

      // --- 3. СИНТЕЗ 10 УНИКАЛЬНЫХ ТЕМБРОВ ---
      for (int c = 0; c < 4; c++) {
        phChord[c] += (freqChord[c] * slowMoFactor) / SAMPLE_RATE;
        if (phChord[c] >= 1.0f) phChord[c] -= 1.0f;
      }
      float chordSig = (fastSinNorm(phChord[0]) + 0.25f * fastSinNorm(phChord[0] * 2.0f) +
                        fastSinNorm(phChord[1]) * 0.78f +
                        fastSinNorm(phChord[2]) * 0.68f +
                        fastSinNorm(phChord[3]) * (song.chordVoicing > 0 ? 0.32f : 0.14f)) * 0.56f * envChord;

      // Лёгкое вибрато для Blade Runner и Ambient Drone
      phVib += 4.2f / SAMPLE_RATE;
      if (phVib >= 1.0f) phVib -= 1.0f;
      float vibMul = (style == 6 || style == 5) ? (1.0f + 0.0045f * fastSinNorm(phVib)) : 1.0f;

      float fLeadNow = curFreqLead * slowMoFactor * vibMul;
      phLead += fLeadNow / SAMPLE_RATE;
      if (phLead >= 1.0f) phLead -= 1.0f;

      phLeadDetune += (fLeadNow * 1.0045f) / SAMPLE_RATE;
      if (phLeadDetune >= 1.0f) phLeadDetune -= 1.0f;

      float s1 = fastSinNorm(phLead);
      float s2 = fastSinNorm(phLead * 2.0f);
      float s3 = fastSinNorm(phLead * 3.0f);
      float s4 = fastSinNorm(phLead * 4.0f);
      float s5 = fastSinNorm(phLead * 5.0f);
      float s6 = fastSinNorm(phLead * 6.0f);
      float sd = fastSinNorm(phLeadDetune);

      float leadSig = 0.0f;
      switch (style) {
        case 0: // LOFI CHILL — электропианино
          leadSig = (s1 + 0.35f * s2 + (0.42f * s4 + 0.22f * s6) * envLead * envLead) * envLead;
          break;
        case 1: // SYNTHWAVE — богатый аналоговый лид с детюном
          leadSig = (0.7f * s1 + 0.45f * sd + 0.55f * s2 + 0.36f * s3 + 0.22f * s4 * envLead) * 0.68f * envLead;
          break;
        case 2: // SPACE TRANCE — хрустальный синт
          leadSig = (s1 + 0.60f * s2 + 0.42f * s3 + 0.32f * s4 + 0.24f * s6 * envLead) * 0.70f * envLead;
          break;
        case 3: // DUB TECHNO — квинтовый стаб
          leadSig = (s1 + 0.55f * fastSinNorm(phLead * 1.5f) + 0.38f * s3 + 0.22f * s5 * envLead) * 0.78f * envLead;
          break;
        case 4: // STALKER POST — звонкая струна
          leadSig = (s1 + 0.68f * s2 + 0.46f * s3 * envLead + (0.38f * s4 + 0.25f * s5) * envLead * envLead) * 0.72f * envLead;
          break;
        case 5: // AMBIENT DRONE — бархатный переливающийся пэд
          leadSig = (0.65f * s1 + 0.55f * sd + 0.30f * fastSinNorm(phLead * 1.5f) + 0.18f * s2) * 0.65f * envLead;
          break;
        case 6: // BLADE RUNNER — тёплый CS-80 брасс с плавным раскрытием обертонов
          leadSig = (0.70f * s1 + 0.50f * sd + (0.55f * s2 + 0.38f * s3 + 0.24f * s4) * envLead) * 0.66f * envLead;
          break;
        case 7: // CYBERPUNK — пробивной нечётный синт
          leadSig = (s1 + 0.62f * s3 + 0.38f * s5 * envLead + 0.35f * sd) * 0.68f * envLead;
          break;
        case 8: // SOLARIS PIANO — стеклянный Rhodes / фетровое пианино
          leadSig = (s1 + 0.28f * s2 + 0.16f * s3 + 0.28f * fastSinNorm(phLead * 7.0f) * envLead * envLead * envLead) * 0.85f * envLead;
          break;
        default: // DEEP HOUSE — тёплый орган/электро-стаб
          leadSig = (0.75f * s1 + 0.52f * s2 + 0.40f * s3 + 0.22f * fastSinNorm(phLead * 1.5f)) * 0.68f * envLead;
          break;
      }

      float kickFreq = (freqBass * slowMoFactor) + envKick * 110.0f;
      phBass += kickFreq / SAMPLE_RATE;
      if (phBass >= 1.0f) phBass -= 1.0f;
      float bassSig = (fastSinNorm(phBass) + fastSinNorm(phBass * 2.0f) * 0.50f +
                       fastSinNorm(phBass * 3.0f) * 0.22f * envKick) *
                      ((style == 5 ? 0.32f : 0.44f) + envKick * 0.70f);

      float synthOut = (chordSig * 0.40f + leadSig * 0.56f + bassSig * 0.42f) * 11500.0f;

      // --- 4. МУЗЫКАЛЬНАЯ DSP-ЦЕПОЧКА ---
      float processed = processMusicalDsp(synthOut, state.dspMode, sp.delayFb);

      // Ультра-низкий DC-блокер (~5 Гц)
      dcBlockState += 0.0015f * (processed - dcBlockState);
      float cleanSignal = processed - dcBlockState;

      float scaled = cleanSignal * smoothVol;
      if (scaled > 26000.0f) {
        scaled = 26000.0f + 6500.0f * ((scaled - 26000.0f) / (6500.0f + (scaled - 26000.0f)));
      } else if (scaled < -26000.0f) {
        scaled = -26000.0f - 6500.0f * ((-scaled - 26000.0f) / (6500.0f + (-scaled - 26000.0f)));
      }

      int16_t finalVal = (int16_t)constrain(scaled, -32500.0f, 32500.0f);
      outBuf[i * 2]     = finalVal;
      outBuf[i * 2 + 1] = finalVal;
    }

    size_t bytesWritten = 0;
    if (txHandle) {
      i2s_channel_write(txHandle, outBuf, sizeof(outBuf), &bytesWritten, portMAX_DELAY);
    }
  }
}

void audioStartTask() {
  xTaskCreatePinnedToCore(audioGenerativeTask, "SynthTask", 8192, NULL, 5, NULL, 0);
}