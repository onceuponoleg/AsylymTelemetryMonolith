#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "config.h"
#include "types.h"
#include "controls.h"
#include "matrix_fx.h"
#include "net_manager.h"
#include "ui.h"

// Глобальное состояние и буферы графиков
SystemState state;
uint8_t histCpu[GRAPH_W]  = {0};
uint8_t histRam[GRAPH_W]  = {0};
uint8_t histDisk[GRAPH_W] = {0};
uint8_t histNet[GRAPH_W]  = {0};

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(150);

  matrixInit();
  controlsInit();
  uiInit();
  uiRunBootSequence();
}

void loop() {
  networkPoll();
  controlsPoll();

  static uint32_t lastMatrixMs = 0;
  static uint32_t lastGraphMs  = 0;
  static uint32_t lastUiMs     = 0;
  uint32_t now = millis();

  if (now - lastGraphMs >= 250) {
    lastGraphMs = now;
    networkPushTelemetryGraphs();
  }

  uint32_t uiInterval = (state.mode == 0) ? 65 : 95;

  if (now - lastMatrixMs >= 30) {
    lastMatrixMs = now;
    matrixRender();
  } else if (now - lastUiMs >= uiInterval) {
    lastUiMs = now;
    uiRender();
  }
}