#include "controls.h"
#include "config.h"
#include "types.h"
#include "themes.h"
#include "net_manager.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// Квадратурная таблица кода Грея
static const int8_t ENC_LUT[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

struct TimerEncoder {
  gpio_num_t pinA;
  gpio_num_t pinB;
  uint8_t    candAB;       // текущее сырое состояние для 1-мс фильтра дребезга
  uint8_t    candCount;    // сколько тиков по 500 мкс оно держится
  uint8_t    stableAB;     // отфильтрованное состояние
  uint8_t    restAB;       // состояние покоя на щелчке (обычно 3 = 11)
  int8_t     accum;        // накопленные четверть-шаги (-4..+4)
  int16_t    detents;      // готовые щелчки для main loop
  uint32_t   lastMoveUs;
};

static portMUX_TYPE encMux = portMUX_INITIALIZER_UNLOCKED;
static esp_timer_handle_t encTimer = nullptr;

static TimerEncoder enc1 = {(gpio_num_t)ENC1_A, (gpio_num_t)ENC1_B, 3, 0, 3, 3, 0, 0, 0};
static TimerEncoder enc2 = {(gpio_num_t)ENC2_A, (gpio_num_t)ENC2_B, 3, 0, 3, 3, 0, 0, 0};

static inline void sampleEncoder(TimerEncoder& e, uint32_t nowUs) {
  uint8_t rawAB = ((uint8_t)gpio_get_level(e.pinA) << 1) | (uint8_t)gpio_get_level(e.pinB);

  // 1-мс фильтр дребезга (2 тика по 500 мкс): отсекает микро-дребезг и постукивания,
  // но гарантированно ловит каждый реальный шаг даже при очень быстром вращении
  if (rawAB == e.candAB) {
    if (e.candCount < 2) e.candCount++;
  } else {
    e.candAB    = rawAB;
    e.candCount = 1;
  }

  if (e.candCount >= 2 && rawAB != e.stableAB) {
    int8_t step = ENC_LUT[(e.stableAB << 2) | rawAB];
    e.stableAB  = rawAB;

    if (step != 0) {
      e.accum      += step;
      e.lastMoveUs  = nowUs;

      // Полный щелчок (4 фазы) или возврат в точку покоя с 3+ фазами
      if (e.accum >= 4) {
        e.detents += ENC_DIR;
        e.accum   -= 4;
      } else if (e.accum <= -4) {
        e.detents -= ENC_DIR;
        e.accum   += 4;
      } else if (rawAB == e.restAB) {
        if (e.accum >= 3)       { e.detents += ENC_DIR; e.accum = 0; }
        else if (e.accum <= -3) { e.detents -= ENC_DIR; e.accum = 0; }
        else if (e.accum == 0)  { /* чистый возврат после касания */ }
      }
    }
  } else if (e.accum != 0 && (nowUs - e.lastMoveUs) > 350000UL) {
    // Сбрасываем «хвост» только после 350 мс полной неподвижности (не мешает медленному кручению!)
    e.accum  = 0;
    e.restAB = e.stableAB;
  }
}

static void onEncTimer(void* arg) {
  (void)arg;
  uint32_t nowUs = (uint32_t)esp_timer_get_time();
  portENTER_CRITICAL(&encMux);
  sampleEncoder(enc1, nowUs);
  sampleEncoder(enc2, nowUs);
  portEXIT_CRITICAL(&encMux);
}

// Программный антидребезг кнопки с защитой от постукиваний
struct DebouncedBtn {
  uint8_t  pin;
  bool     rawLast      = false;
  bool     stableState  = false;
  uint32_t rawChangeMs  = 0;
  uint32_t pressStartMs = 0;
  uint32_t lastEventMs  = 0;
  bool     longHandled  = false;
};

static DebouncedBtn btn1 = {ENC1_SW};
static DebouncedBtn btn2 = {ENC2_SW};
static uint32_t dualHoldMs = 0;

static void updateButton(DebouncedBtn& b, uint32_t now, bool otherHeld,
                         void (*onShortClick)(), void (*onLongPress)()) {
  bool raw = (digitalRead(b.pin) == LOW);
  if (raw != b.rawLast) {
    b.rawLast     = raw;
    b.rawChangeMs = now;
  }

  if ((now - b.rawChangeMs) >= 35) {
    if (raw != b.stableState) {
      b.stableState = raw;
      b.lastEventMs = now;
      if (raw) {
        b.pressStartMs = now;
        b.longHandled  = false;
      } else if (!b.longHandled && !otherHeld && (now - b.pressStartMs) < 600) {
        onShortClick();
      }
    } else if (b.stableState && !b.longHandled && !otherHeld && (now - b.pressStartMs) >= 650) {
      b.longHandled = true;
      onLongPress();
    }
  }
}

static void onEnc1Click() {
  state.enc1Focus = (state.enc1Focus + 1) % 3;
  if (state.enc1Focus == 1) state.lastWaveTouchMs = millis();
}
static void onEnc1Long()  { state.muted = !state.muted; }
static void onEnc2Click() { state.enc2Focus = (state.enc2Focus + 1) % 3; }
static void onEnc2Long()  { state.lightOff = !state.lightOff; }

void controlsInit() {
  pinMode(ENC1_A, INPUT_PULLUP);
  pinMode(ENC1_B, INPUT_PULLUP);
  pinMode(ENC1_SW, INPUT_PULLUP);
  pinMode(ENC2_A, INPUT_PULLUP);
  pinMode(ENC2_B, INPUT_PULLUP);
  pinMode(ENC2_SW, INPUT_PULLUP);
  pinMode(SW_POS_ANIM, INPUT_PULLUP);
  pinMode(SW_POS_SYNC, INPUT_PULLUP);

  uint8_t init1 = ((uint8_t)digitalRead(ENC1_A) << 1) | (uint8_t)digitalRead(ENC1_B);
  uint8_t init2 = ((uint8_t)digitalRead(ENC2_A) << 1) | (uint8_t)digitalRead(ENC2_B);
  enc1.candAB = enc1.stableAB = enc1.restAB = init1;
  enc2.candAB = enc2.stableAB = enc2.restAB = init2;

  esp_timer_create_args_t tArgs = {};
  tArgs.callback = &onEncTimer;
  tArgs.name     = "enc_poll";
  ESP_ERROR_CHECK(esp_timer_create(&tArgs, &encTimer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(encTimer, 500));  // 2 кГц
}

bool controlsBothButtonsHeld() {
  return (digitalRead(ENC1_SW) == LOW && digitalRead(ENC2_SW) == LOW);
}

void controlsPoll() {
  // 1. Тумблер 3 позиции
  if (digitalRead(SW_POS_ANIM) == LOW)      state.mode = 0;  // [CAT]
  else if (digitalRead(SW_POS_SYNC) == LOW) state.mode = 2;  // [SYNC]
  else                                      state.mode = 1;  // [CLEAN]

  uint32_t now = millis();

  // 2. Кнопки энкодеров
  bool sw1Raw = (digitalRead(ENC1_SW) == LOW);
  bool sw2Raw = (digitalRead(ENC2_SW) == LOW);

  if (sw1Raw && sw2Raw) {
    if (dualHoldMs == 0) dualHoldMs = now;
    else if (!state.apConfigMode && (now - dualHoldMs > 3000)) {
      networkStartPortal();
      dualHoldMs = 0;
      return;
    }
  } else {
    dualHoldMs = 0;
  }

  updateButton(btn1, now, btn2.stableState, onEnc1Click, onEnc1Long);
  updateButton(btn2, now, btn1.stableState, onEnc2Click, onEnc2Long);

  // 3. Атомарно забираем накопленные щелчки из 2-кГц таймера
  portENTER_CRITICAL(&encMux);
  int16_t d1 = enc1.detents; enc1.detents = 0;
  int16_t d2 = enc2.detents; enc2.detents = 0;
  portEXIT_CRITICAL(&encMux);

  // Игнорируем случайный поворот в момент нажатия/отпускания самой кнопки
  if (btn1.stableState || (now - btn1.lastEventMs < 85)) d1 = 0;
  if (btn2.stableState || (now - btn2.lastEventMs < 85)) d2 = 0;

  if (d1 != 0) {
    if (state.enc1Focus == 0) {
      state.volPct = (uint8_t)constrain((int)state.volPct + d1 * 2, 0, 100);
      if (state.volPct > 0) state.muted = false;
    } else if (state.enc1Focus == 1) {
      state.stationIdx = (state.stationIdx + (d1 > 0 ? 1 : NUM_STATIONS - 1)) % NUM_STATIONS;
      state.reseedRequested = true;
      state.lastWaveTouchMs = now;
    } else {
      state.dspMode = (state.dspMode + (d1 > 0 ? 1 : NUM_DSP - 1)) % NUM_DSP;
    }
  }

  if (d2 != 0) {
    if (state.enc2Focus == 0) {
      state.fxMode = (state.fxMode + (d2 > 0 ? 1 : NUM_FX - 1)) % NUM_FX;
    } else if (state.enc2Focus == 1) {
      state.colorIdx = (state.colorIdx + (d2 > 0 ? 1 : NUM_COLORS - 1)) % NUM_COLORS;
    } else {
      state.brightPct = (uint8_t)constrain((int)state.brightPct + d2 * 5, 0, 100);
      if (state.brightPct > 0) state.lightOff = false;
    }
  }
}