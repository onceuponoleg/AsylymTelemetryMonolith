#include "net_manager.h"
#include "config.h"
#include "types.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <math.h>

static WiFiUDP*   udp       = nullptr;
static WebServer* webServer = nullptr;
static DNSServer* dnsServer = nullptr;
static Preferences prefs;

constexpr int MAX_SCAN_NETS = 12;
static String scannedSsids[MAX_SCAN_NETS];
static int    scannedRssi[MAX_SCAN_NETS];
static int    scannedCount = 0;

static void scanWiFiNetworksClean() {
  int n = WiFi.scanNetworks(false, false);
  scannedCount = 0;
  for (int i = 0; i < n && scannedCount < MAX_SCAN_NETS; i++) {
    String s = WiFi.SSID(i);
    s.trim();
    if (s.length() == 0) continue;

    bool dup = false;
    for (int j = 0; j < scannedCount; j++) {
      if (scannedSsids[j] == s) { dup = true; break; }
    }
    if (!dup) {
      scannedSsids[scannedCount] = s;
      scannedRssi[scannedCount]  = WiFi.RSSI(i);
      scannedCount++;
    }
  }
  WiFi.scanDelete();
}

static void handlePortalRoot() {
  if (!webServer) return;
  if (webServer->hasArg("rescan")) scanWiFiNetworksClean();

  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ASYLYM MONOLITH // WI-FI</title>
<style>
body{background:#02080d;color:#00e5ff;font-family:monospace;padding:18px;max-width:440px;margin:auto;}
.box{border:1px solid #00e5ff;padding:16px;background:#06131c;}
h2{color:#ff6b00;border-bottom:1px solid #00e5ff;padding-bottom:8px;margin-top:0;}
label{display:block;margin-top:12px;font-size:13px;}
select,input,button{width:100%;padding:12px;margin:6px 0;background:#02080d;color:#00e5ff;border:1px solid #00e5ff;font-family:monospace;box-sizing:border-box;font-size:14px;}
select{color:#ff6b00;border-color:#ff6b00;}
button{background:#ff6b00;color:#02080d;border-color:#ff6b00;font-weight:bold;cursor:pointer;margin-top:14px;}
.btn-sec{background:#0a2133;color:#00e5ff;border-color:#0a4a58;margin-top:6px;}
</style>
<script>function pickNet(v){if(v){document.getElementById('ssid').value=v;document.getElementById('pass').focus();}}</script>
</head><body><div class="box">
<h2>ASYLYM MONOLITH // WI-FI</h2>
<label>1. Choose network:</label>
<select onchange="pickNet(this.value)">
<option value="">-- Networks found: )rawliteral" + String(scannedCount) + R"rawliteral( --</option>
)rawliteral";

  for (int i = 0; i < scannedCount; i++) {
    String safeSsid = scannedSsids[i];
    safeSsid.replace("\"", "&quot;");
    html += "<option value=\"" + safeSsid + "\">" + safeSsid + " (" + String(scannedRssi[i]) + " dBm)</option>";
  }

  html += R"rawliteral(
</select>
<form method="POST" action="/save">
<label>2. SSID:</label>
<input id="ssid" name="ssid" placeholder="Wi-Fi SSID" required>
<label>3. ПАРОЛЬ:</label>
<input id="pass" name="pass" type="password" placeholder="Wi-Fi Password">
<button type="submit">[ SAVE & REBOOT ]</button>
</form>
<form method="GET" action="/"><input type="hidden" name="rescan" value="1">
<button type="submit" class="btn-sec">RESCAN NETWORKS</button></form>
</div></body></html>
)rawliteral";

  webServer->send(200, "text/html", html);
}

static void handlePortalSave() {
  if (!webServer) return;
  String ssid = webServer->arg("ssid");
  String pass = webServer->arg("pass");
  ssid.trim();

  if (ssid.length() > 0) {
    prefs.begin("aether", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    webServer->send(200, "text/html",
      "<html><body style='background:#02080d;color:#ff6b00;font-family:monospace;text-align:center;padding:30px'>"
      "<h2>[OK] SAVED: " + ssid + "</h2><p>Rebooting...</p></body></html>");
    delay(2000);
    ESP.restart();
  } else {
    webServer->sendHeader("Location", "/");
    webServer->send(302);
  }
}

void networkLoadCredentials(String& outSsid, String& outPass) {
  prefs.begin("aether", true);
  String savedSsid = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();

  outSsid = (savedSsid.length() > 0) ? savedSsid : String(DEBUG_WIFI_SSID);
  outPass = (savedSsid.length() > 0) ? savedPass : String(DEBUG_WIFI_PASS);
}

void networkBeginSta(const String& ssid, const String& pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
}

void networkStartServices(const String& connectedSsid) {
  state.apConfigMode = false;
  state.activeSsid   = connectedSsid;

  if (!udp) udp = new WiFiUDP();
  udp->begin(UDP_PORT);

  MDNS.begin(MDNS_HOST);
  ArduinoOTA.setHostname(MDNS_HOST);
  ArduinoOTA.begin();
}

void networkStartPortal() {
  state.apConfigMode = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  scanWiFiNetworksClean();
  WiFi.softAP(AP_SSID);

  if (!dnsServer) dnsServer = new DNSServer();
  if (!webServer) webServer = new WebServer(80);

  dnsServer->start(53, "*", WiFi.softAPIP());
  webServer->on("/", handlePortalRoot);
  webServer->on("/save", HTTP_POST, handlePortalSave);
  webServer->onNotFound(handlePortalRoot);
  webServer->begin();
}

static void handleUdp() {
  if (state.apConfigMode || !udp) return;
  int packetSize = udp->parsePacket();
  if (!packetSize) return;

  char buf[512];
  int len = udp->read(buf, sizeof(buf) - 1);
  if (len <= 0) return;
  buf[len] = '\0';

  JsonDocument doc;
  if (deserializeJson(doc, buf) == DeserializationError::Ok) {
    state.cpuLoad      = doc["cpu"] | 0;
    state.ramPct       = doc["ram"] | 0;
    state.swapGb       = doc["swap_gb"] | 0.0f;
    state.swapAlert    = doc["swap_alert"] | false;
    state.diskIoMbs    = doc["disk_mbs"] | 0.0f;
    state.netKbs       = doc["net_kbs"] | 0.0f;
    state.cpuTemp      = doc["temp"] | 0;
    state.thermalAlert = doc["therm_alert"] | false;
    state.pingMs       = doc["ping"] | 0;
    state.netAlert     = doc["net_alert"] | false;
    strlcpy(state.topHog, doc["hog"]  | "IDLE", sizeof(state.topHog));
    strlcpy(state.osType, doc["os"]   | "PC",   sizeof(state.osType));
    strlcpy(state.pcTime, doc["time"] | "",     sizeof(state.pcTime));
    strlcpy(state.pcDate, doc["date"] | "",     sizeof(state.pcDate));
    state.lastTelemetryMs = millis();
  }
}

void networkPoll() {
  if (state.apConfigMode) {
    if (dnsServer) dnsServer->processNextRequest();
    if (webServer) webServer->handleClient();
  } else {
    ArduinoOTA.handle();
    handleUdp();
  }
}

void networkPushTelemetryGraphs() {
  for (int i = 0; i < GRAPH_W - 1; i++) {
    histCpu[i]  = histCpu[i + 1];
    histRam[i]  = histRam[i + 1];
    histDisk[i] = histDisk[i + 1];
    histNet[i]  = histNet[i + 1];
  }

  if (isPcOnline()) {
    histCpu[GRAPH_W - 1]  = (uint8_t)constrain(state.cpuLoad, 0, 100);
    histRam[GRAPH_W - 1]  = (uint8_t)constrain(state.ramPct, 0, 100);
    int diskScaled = (state.diskIoMbs <= 0.1f) ? 2 : constrain((int)(sqrtf(state.diskIoMbs) * 8.5f), 4, 100);
    histDisk[GRAPH_W - 1] = (uint8_t)diskScaled;
    int netScaled  = (state.netKbs <= 1.0f) ? 2 : constrain((int)(sqrtf(state.netKbs) * 1.4f), 4, 100);
    histNet[GRAPH_W - 1]  = (uint8_t)netScaled;
  } else {
    uint8_t idleWave = 8 + (sin8(millis() / 12) >> 5);
    histCpu[GRAPH_W - 1]  = idleWave;
    histRam[GRAPH_W - 1]  = idleWave;
    histDisk[GRAPH_W - 1] = 4;
    histNet[GRAPH_W - 1]  = 4;
  }
}