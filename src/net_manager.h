#pragma once
#include <Arduino.h>

void networkLoadCredentials(String& outSsid, String& outPass);
void networkBeginSta(const String& ssid, const String& pass);
void networkStartServices(const String& connectedSsid);
void networkStartPortal();
void networkPoll();
void networkPushTelemetryGraphs();