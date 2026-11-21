#include "ui.h"
#include "config.h"
#include "types.h"
#include "themes.h"
#include "controls.h"
#include "matrix_fx.h"
#include "audio_engine.h"
#include "net_manager.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <math.h>

static TFT_eSPI    tft = TFT_eSPI();
static TFT_eSprite spr = TFT_eSprite(&tft);

// ================= БУТСКРИН (5 ЭТАПОВ) =================
static void drawBootHeader(uint8_t stageNum, const char* title, int pct) {
  const ColorTheme& th = activeTheme();
  spr.fillSprite(th.uiBg);
  spr.drawRect(UI_X0, UI_Y0, UI_W, UI_H, th.uiDim);
  spr.drawFastHLine(UI_X0, UI_Y0 + 21, UI_W, th.uiDim);

  spr.setTextColor(th.uiAccent, th.uiBg);
  spr.drawString("BOOT " + String(stageNum) + "/5 // " + String(title), UI_X0 + 6, UI_Y0 + 3, 2);

  int barX = UI_X0 + 8;
  int barY = UI_Y0 + UI_H - 18;
  int barMaxW = UI_W - 16;
  spr.drawRect(barX, barY, barMaxW, 11, th.uiDim);
  int barW = map(constrain(pct, 0, 100), 0, 100, 0, barMaxW - 4);
  spr.fillRect(barX + 2, barY + 2, barW, 7, th.uiAccent);
}

void uiInit() {
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_TFT_BLK, OUTPUT);
  digitalWrite(PIN_TFT_BLK, HIGH);

  const ColorTheme& th = activeTheme();
  tft.init();
  tft.setRotation(3);  // Поворот на 180°
  tft.fillScreen(th.uiBg);

  spr.setColorDepth(8);
  spr.createSprite(320, 170);
}

void uiRunBootSequence() {
  const ColorTheme& th = activeTheme();

  // 1. Система
  uint32_t t0 = millis();
  while (millis() - t0 < 1300) {
    uint32_t el = millis() - t0;
    drawBootHeader(1, "CORE DIAGNOSTICS", (el * 100) / 1300);
    spr.setTextColor(th.uiAccent, th.uiBg);
    spr.drawCentreString("A S Y L Y M // M O N O L I T H", 160, 42, 2);
    spr.setTextColor(th.uiMain, th.uiBg);
    spr.drawString("FREE RAM : " + String(ESP.getFreeHeap() / 1024) + " KB", UI_X0 + 10, 72, 2);
    spr.setTextColor(th.uiDim, th.uiBg);
    spr.drawString("10 STYLES | 6 DSP | 8 AMBIENT FX", UI_X0 + 10, 96, 2);
    spr.pushSprite(0, 0);
    delay(30);
  }

  // 2. Матрица
  t0 = millis();
  while (millis() - t0 < 1500) {
    uint32_t el = millis() - t0;
    matrixBootSweep(el);
    drawBootHeader(2, "ARC MATRIX CHECK", (el * 100) / 1500);
    spr.setTextColor(th.uiMain, th.uiBg);
    spr.drawString("WS2812B 32x8 AMBIENT ENGINE OK", UI_X0 + 10, 54, 2);
    spr.setTextColor(th.uiDim, th.uiBg);
    spr.drawString("THEME: " + String(th.name) + " | MAX: " + String(MAX_CURRENT_MA) + "mA", UI_X0 + 10, 82, 2);
    spr.pushSprite(0, 0);
    delay(25);
  }
  matrixClear();

  // 3. Аудио
  audioInitHardware();
  const float freqs[3] = {440.0f, 554.3f, 659.2f};
  for (int i = 0; i < 3; i++) {
    drawBootHeader(3, "AUDIO SYNTH & DSP", ((i + 1) * 100) / 3);
    spr.setTextColor(th.uiMain, th.uiBg);
    spr.drawString("MAX98357A I2S : " + String((int)freqs[i]) + " Hz", UI_X0 + 10, 56, 2);
    spr.pushSprite(0, 0);
    audioPlayTestTone(freqs[i], 150);
  }
  audioStartTask();

  // 4. Wi-Fi
  String ssid, pass;
  networkLoadCredentials(ssid, pass);
  bool forcePortal = controlsBothButtonsHeld();
  if (!forcePortal && ssid.length() > 0) {
    networkBeginSta(ssid, pass);
  }

  t0 = millis();
  while (millis() - t0 < 5500) {
    uint32_t el = millis() - t0;
    bool ok = (WiFi.status() == WL_CONNECTED);
    drawBootHeader(4, "WI-FI LINK", (el * 100) / 5500);
    spr.setTextColor(th.uiDim, th.uiBg);
    spr.drawString("SSID : " + ssid, UI_X0 + 10, 44, 2);
    if (ok) {
      spr.setTextColor(th.uiAccent, th.uiBg);
      spr.drawString("CONNECTED: " + WiFi.localIP().toString(), UI_X0 + 10, 76, 2);
      spr.pushSprite(0, 0);
      delay(400);
      break;
    } else {
      spr.setTextColor(th.uiMain, th.uiBg);
      spr.drawString("CONNECTING...", UI_X0 + 10, 76, 2);
      spr.pushSprite(0, 0);
    }
    delay(100);
  }

  // 5. Сервисы
  if (WiFi.status() == WL_CONNECTED && !forcePortal) {
    networkStartServices(ssid);
  } else {
    networkStartPortal();
  }
}

// ================= ПИКСЕЛЬНАЯ СЦЕНА РЕЖИМА [CAT] =================
static const char* const CAT_WALK1[11] = {
  "..........#...#.", ".#........#####.", "..#.......#A#A#.", "..#.......#####.",
  "..#..########...", "...###########..", "....##########..", "....##########..",
  "....##......##..", "...##........##.", "...##........##."
};
static const char* const CAT_WALK2[11] = {
  "..........#...#.", "..#.......#####.", "...#......#A#A#.", "...#......#####.",
  "...#.########...", "....##########..", "....##########..", "....##########..",
  ".....##....##...", "......##..##....", "......##..##...."
};
static const char* const CAT_SIT[11] = {
  "........#...#...", "........#####...", "........#A#A#...", "........#####...",
  ".......######...", ".#....#######...", "..#..########...", "..#..########...",
  "...##########...", "....#########...", "....########...."
};
static const char* const CAT_SLEEP[11] = {
  "................", "................", "................", "................",
  "................", ".........#..#...", "....#########...", "...###########..",
  "..#######A#A##..", "..############..", "...##########..."
};

static void drawCatSprite(int x, int y, const char* const rows[11], bool facingRight, bool blink, const ColorTheme& th) {
  constexpr int S = 2;
  for (int r = 0; r < 11; r++) {
    for (int c = 0; c < 16; c++) {
      char ch = rows[r][c];
      if (ch == '.') continue;
      int drawCol = facingRight ? c : (15 - c);
      uint16_t col = (ch == 'A' && !blink) ? th.uiAccent : th.uiMain;
      spr.fillRect(x + drawCol * S, y + r * S, S, S, col);
    }
  }
}

static void drawPixelCatScene(int areaX, int areaY, int areaW, int areaH, const ColorTheme& th) {
  static float    catX         = 60.0f;
  static int8_t   catDir       = 1;
  static uint8_t  catAction    = 0;  // 0=walk, 1=sit, 2=sleep
  static uint32_t nextActionMs = 5000;
  static uint32_t lastStepMs   = 0;
  static uint8_t  walkFrame    = 0;
  static bool     inited       = false;

  uint32_t now = millis();
  if (!inited) {
    inited = true;
    catX = areaX + 36.0f;
    nextActionMs = now + 4500;
  }

  int groundY = areaY + areaH - 12;

  // Звёзды
  static const int16_t starX[12] = {18, 44, 76, 108, 134, 165, 192, 220, 248, 276, 88, 206};
  static const int16_t starY[12] = {10, 22, 12, 26,  9,   19,  11,  24,  14,  28,  34, 32};
  for (int i = 0; i < 12; i++) {
    bool twinkle = (((now / 320) + i * 3) % 7) == 0;
    int sx = areaX + starX[i];
    int sy = areaY + starY[i];
    if (twinkle) spr.fillRect(sx, sy, 2, 2, th.uiAccent);
    else         spr.drawPixel(sx, sy, th.uiDim);
  }

  // Полумесяц
  int moonX = areaX + areaW - 28;
  int moonY = areaY + 14;
  spr.fillCircle(moonX, moonY, 8, th.uiAccent);
  spr.fillCircle(moonX + 4, moonY - 2, 7, th.uiBg);

  // Горизонт башен
  static const int16_t bldX[7] = {12, 46, 92, 142, 196, 238, 268};
  static const int16_t bldW[7] = {24, 32, 28, 36,  26,  22,  24};
  static const int16_t bldH[7] = {26, 36, 20, 40,  28,  22,  32};
  for (int i = 0; i < 7; i++) {
    int bx = areaX + bldX[i];
    int by = groundY - bldH[i];
    spr.fillRect(bx, by, bldW[i], bldH[i], th.uiGrid);
    for (int wy = by + 4; wy < groundY - 6; wy += 6) {
      for (int wx = bx + 4; wx < bx + bldW[i] - 5; wx += 7) {
        if (((wx + wy + i * 5) & 3) == 0) spr.fillRect(wx, wy, 2, 2, th.uiDim);
      }
    }
  }

  // Шпиль + маяк
  int spireX = areaX + bldX[3] + bldW[3] / 2;
  int spireTop = groundY - bldH[3] - 10;
  spr.drawFastVLine(spireX, spireTop, 10, th.uiDim);
  if ((now / 700) & 1) spr.fillRect(spireX - 1, spireTop - 1, 3, 2, th.uiAccent);

  // Декор сцены от активного музыкального DSP-режима
  if (state.dspMode == 5) {
    // SPACE: падающая звезда
    int stP = (now / 45) % 90;
    if (stP < 36) {
      int sx = areaX + 20 + stP * 4;
      int sy = areaY + 6 + stP;
      spr.drawLine(sx, sy, sx - 8, sy - 2, th.uiAccent);
    }
  } else if (state.dspMode == 6 || state.dspMode == 3) {
    // SWEEP / PHASER: мягкие радарные круги от шпиля
    int ringR = (now / 65) % 26;
    if (ringR > 3) spr.drawCircle(spireX, spireTop, ringR, (ringR < 14) ? th.uiDim : th.uiGrid);
  } else if ((state.dspMode == 2 || state.dspMode == 4) && !state.muted) {
    // TAPE / TREMOLO: плывущая пиксельная нота
    int nx = areaX + 26 + (int)(sinf(now * 0.0018f) * 8.0f);
    int ny = areaY + 26 - ((now / 110) % 14);
    spr.fillRect(nx, ny + 4, 4, 3, th.uiAccent);
    spr.drawFastVLine(nx + 3, ny - 3, 8, th.uiAccent);
    spr.drawFastHLine(nx + 3, ny - 3, 4, th.uiAccent);
  } else if (state.dspMode == 1) {
    // BUNKER: уютный фонарь-маячок на земле слева
    int lx = areaX + 18;
    spr.drawFastVLine(lx, groundY - 16, 16, th.uiDim);
    spr.fillRect(lx - 2, groundY - 19, 5, 4, th.uiAccent);
  }

  // Земля
  spr.drawFastHLine(areaX + 2, groundY, areaW - 4, th.uiDim);
  for (int gx = areaX + 8; gx < areaX + areaW - 8; gx += 18) {
    spr.drawFastHLine(gx, groundY + 4, 8, th.uiGrid);
    spr.drawFastHLine(gx + 6, groundY + 8, 5, th.uiGrid);
  }

  // Светлячок
  float flyT = now * 0.0014f;
  int flyX = areaX + areaW / 2 + (int)(sinf(flyT) * (areaW * 0.34f));
  int flyY = areaY + 34 + (int)(sinf(flyT * 2.1f) * 16.0f);
  bool wingUp = ((now / 140) & 1) != 0;
  spr.fillRect(flyX, flyY, 2, 2, th.uiAccent);
  spr.fillRect(flyX - 2, flyY + (wingUp ? -2 : 1), 2, 2, th.uiMain);
  spr.fillRect(flyX + 2, flyY + (wingUp ? -2 : 1), 2, 2, th.uiMain);

  // Котик
  int minCatX = areaX + ((state.dspMode == 1) ? 28 : 8);
  int maxCatX = areaX + areaW - 40;

  if (now >= nextActionMs) {
    if (catAction == 0) {
      catAction = (random(0, 100) < 32) ? 2 : 1;
      nextActionMs = now + (catAction == 2 ? random(3800, 6200) : random(2000, 3800));
    } else {
      catAction = 0;
      if (random(0, 100) < 35) catDir = -catDir;
      nextActionMs = now + random(4200, 8500);
    }
  }

  if (catAction == 0) {
    catX += catDir * 1.45f;
    if (catX >= maxCatX)      { catX = maxCatX; catDir = -1; }
    else if (catX <= minCatX) { catX = minCatX; catDir =  1; }
    if (now - lastStepMs >= 190) {
      lastStepMs = now;
      walkFrame ^= 1;
    }
  } else if (catAction == 1) {
    catDir = (flyX >= (int)catX + 16) ? 1 : -1;
  }

  bool blink = ((now % 3600) < 180);
  int catDrawX = (int)catX;
  int catDrawY = groundY - 22;

  if (catAction == 0)      drawCatSprite(catDrawX, catDrawY, walkFrame ? CAT_WALK2 : CAT_WALK1, catDir > 0, blink, th);
  else if (catAction == 1) drawCatSprite(catDrawX, catDrawY, CAT_SIT, catDir > 0, blink, th);
  else {
    drawCatSprite(catDrawX, catDrawY, CAT_SLEEP, catDir > 0, true, th);
    int zPhase = (now / 450) % 3;
    int zx = catDrawX + (catDir > 0 ? 22 : 4);
    int zy = catDrawY + 4 - zPhase * 5;
    spr.setTextColor(zPhase == 2 ? th.uiDim : th.uiAccent, th.uiBg);
    spr.drawString(zPhase == 0 ? "z" : "Z", zx + zPhase * 3, zy, 1);
  }
}

// ================= ОСЦИЛЛОГРАФЫ И ОСНОВНОЙ ЭКРАН =================
static void drawOscilloscopePanel(int x, int y, int w, int h, const String& title,
                                  const uint8_t* hist, const ColorTheme& th, bool alert) {
  uint16_t borderCol = alert ? th.uiAlert : th.uiDim;
  uint16_t activeCol = alert ? th.uiAlert : th.uiMain;

  spr.drawRect(x, y, w, h, borderCol);

  int midY = y + 14 + (h - 16) / 2;
  for (int gx = x + 6; gx < x + w - 4; gx += 12) {
    spr.drawPixel(gx, midY, th.uiGrid);
    spr.drawPixel(gx, y + h - 9, th.uiGrid);
  }

  spr.setTextColor(activeCol, th.uiBg);
  spr.drawString(title, x + 4, y + 1, 2);

  int graphBottom = y + h - 2;
  int graphHeight = h - 17;

  for (int i = 0; i < GRAPH_W - 1; i++) {
    int px1 = x + 4 + i;
    int py1 = graphBottom - ((int)hist[i] * graphHeight) / 100;
    int py2 = graphBottom - ((int)hist[i + 1] * graphHeight) / 100;

    if ((i & 1) == 0 && py1 < graphBottom) {
      spr.drawFastVLine(px1, py1 + 1, graphBottom - py1, th.uiGrid);
    }
    spr.drawLine(px1, py1, px1 + 1, py2, activeCol);
  }
}

void uiRender() {
  const ColorTheme& th = activeTheme();
  uint32_t now = millis();

  spr.fillSprite(th.uiBg);
  spr.drawRect(UI_X0, UI_Y0, UI_W, UI_H, th.uiDim);
  spr.drawFastHLine(UI_X0, UI_Y0 + 20, UI_W, th.uiDim);
  spr.drawFastHLine(UI_X0, UI_Y0 + 123, UI_W, th.uiDim);

  constexpr int contentX = UI_X0 + 4;
  constexpr int rightX   = UI_X0 + UI_W - 5;

  if (state.apConfigMode) {
    spr.setTextColor(th.uiAlert, th.uiBg);
    spr.drawString("[!] WI-FI SETUP PORTAL", contentX, UI_Y0 + 2, 2);
    spr.setTextColor(th.uiMain, th.uiBg);
    spr.drawString("1. Wi-Fi: " + String(AP_SSID), contentX + 2, 36, 2);
    spr.drawString("2. Open : http://192.168.4.1", contentX + 2, 62, 2);
  } else {
    // --- ЛЕВАЯ ЧАСТЬ ШАПКИ ---
    // 2.5с после смены стиля WAV показываем название мелодии,
    // в остальное время: в [CAT] -> ASYLYM MONOLITH, при подключённом ПК -> время с ПК
    bool showWaveToast = (state.lastWaveTouchMs > 0) && (now - state.lastWaveTouchMs < 2600);
    String leftHeader;
    uint16_t leftCol = th.uiMain;

    if (showWaveToast) {
      leftHeader = String(WAVE_NAMES[state.stationIdx]) + " #" + String(state.currentSeedId);
      leftCol    = th.uiAccent;
    } else if (state.mode == 0) {
      leftHeader = "ASYLYM MONOLITH";
      leftCol    = th.uiMain;
    } else if (isPcOnline() && state.pcTime[0] != '\0') {
      leftHeader = String(state.pcTime);
      if (state.pcDate[0] != '\0') leftHeader += "  " + String(state.pcDate);
      leftCol    = th.uiMain;
    } else {
      leftHeader = "ASYLYM MONOLITH";
      leftCol    = th.uiDim;
    }

    spr.setTextColor(leftCol, th.uiBg);
    spr.drawString(leftHeader, contentX, UI_Y0 + 2, 2);

    // --- ПРАВАЯ ЧАСТЬ ШАПКИ ---
    static const char* const modeNames[3] = {"[CAT]", "[CLEAN]", "[SYNC]"};
    spr.setTextColor(state.mode == 1 ? th.uiDim : th.uiAccent, th.uiBg);
    String rightLabel = (state.mode == 0)
                          ? String(modeNames[0])
                          : (String(state.osType) + " " + modeNames[state.mode]);
    spr.drawRightString(rightLabel, rightX, UI_Y0 + 2, 2);

    // --- ЦЕНТРАЛЬНАЯ ЗОНА ---
    if (state.mode == 0) {
      drawPixelCatScene(UI_X0 + 2, UI_Y0 + 22, UI_W - 4, 100, th);
    } else {
      constexpr int panelW = 148;
      constexpr int panelH = 48;
      constexpr int colL   = contentX;
      constexpr int colR   = contentX + panelW + 4;
      constexpr int rowT   = UI_Y0 + 23;
      constexpr int rowB   = rowT + panelH + 2;

      String cpuTitle = "CPU:" + String(state.cpuLoad) + "% " + String(state.topHog).substring(0, 8);
      drawOscilloscopePanel(colL, rowT, panelW, panelH, cpuTitle, histCpu, th, state.thermalAlert);

      String ramTitle = "RAM:" + String(state.ramPct) + "% SWP:" + String(state.swapGb, 1) + "G";
      drawOscilloscopePanel(colR, rowT, panelW, panelH, ramTitle, histRam, th, state.swapAlert);

      String diskTitle = "DSK:" + String(state.diskIoMbs, 1) + "M";
      if (state.cpuTemp > 0) diskTitle += " T:" + String(state.cpuTemp) + "C";
      drawOscilloscopePanel(colL, rowB, panelW, panelH, diskTitle, histDisk, th, state.thermalAlert);

      String netRate = (state.netKbs >= 1024.0f)
                         ? (String(state.netKbs / 1024.0f, 1) + "M")
                         : (String((int)state.netKbs) + "K");
      String netTitle = "NET:" + netRate + " PNG:" + String(state.pingMs) + "ms";
      drawOscilloscopePanel(colR, rowB, panelW, panelH, netTitle, histNet, th, state.netAlert);

      if (!isPcOnline()) {
        spr.fillRect(34, 58, 252, 34, th.uiBg);
        spr.drawRect(34, 58, 252, 34, th.uiAccent);
        spr.setTextColor(th.uiAccent, th.uiBg);
        spr.drawCentreString("WAIT PC UDP :4210 (" + WiFi.localIP().toString() + ")", 160, 61, 2);
        spr.setTextColor(th.uiDim, th.uiBg);
        spr.drawCentreString("Host: aether-monolith.local", 160, 77, 1);
      }
    }
  }

  // --- НИЖНЯЯ СТРОКА СТАТУСА ЭНКОДЕРОВ (ЕДИНАЯ: ПРИГЛУШЁННЫЕ ДО ВЫБОРА) ---
  constexpr int rowEnc1Y = UI_Y0 + 126;
  constexpr int rowEnc2Y = UI_Y0 + 144;

  String volStr = (state.muted || state.volPct == 0) ? "MUTE" : (String(state.volPct) + "%");
  uint16_t volCol = (state.muted || state.volPct == 0)
                      ? th.uiAlert
                      : (state.enc1Focus == 0 ? th.uiAccent : th.uiDim);
  spr.setTextColor(volCol, th.uiBg);
  spr.drawString(String(state.enc1Focus == 0 ? ">VOL:" : " VOL:") + volStr, contentX, rowEnc1Y, 2);

  spr.setTextColor(state.enc1Focus == 1 ? th.uiAccent : th.uiDim, th.uiBg);
  spr.drawString(String(state.enc1Focus == 1 ? ">WAV:" : " WAV:") + String(state.stationIdx + 1) + "/10", 104, rowEnc1Y, 2);

  spr.setTextColor(state.enc1Focus == 2 ? th.uiAccent : th.uiDim, th.uiBg);
  spr.drawString(String(state.enc1Focus == 2 ? ">DSP:" : " DSP:") + DSP_NAMES[state.dspMode], 192, rowEnc1Y, 2);

  spr.setTextColor(state.enc2Focus == 0 ? th.uiAccent : th.uiDim, th.uiBg);
  spr.drawString(String(state.enc2Focus == 0 ? ">FX:" : " FX:") + FX_NAMES[state.fxMode], contentX, rowEnc2Y, 2);

  spr.setTextColor(state.enc2Focus == 1 ? th.uiAccent : th.uiDim, th.uiBg);
  spr.drawString(String(state.enc2Focus == 1 ? ">COL:" : " COL:") + th.name, 104, rowEnc2Y, 2);

  bool isOff = state.lightOff || (state.brightPct == 0);
  String briStr = isOff ? "OFF" : (String(state.brightPct) + "%");
  uint16_t briCol = isOff ? th.uiAlert : (state.enc2Focus == 2 ? th.uiAccent : th.uiDim);
  spr.setTextColor(briCol, th.uiBg);
  spr.drawString(String(state.enc2Focus == 2 ? ">BRI:" : " BRI:") + briStr, 214, rowEnc2Y, 2);

  spr.pushSprite(0, 0);
}