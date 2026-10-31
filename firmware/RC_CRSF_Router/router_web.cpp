// router_web.cpp - CRSF-Router, siehe router_web.h
#include "router_web.h"
#include <WiFi.h>
#include <WebServer.h>
#include "router_config.h"
#include "webui.h"
#include "src/RC_Common/web_common.h"
#include "src/RC_Common/config_backup.h"

static WebServer server(80);
static bool      apActive = false;

static void sendJson(const String& s) { server.send(200, "application/json", s); }
static void sendError(const char* code) {
    server.send(400, "application/json", String("{\"ok\":false,\"error\":\"") + code + "\"}");
}

static String configJson() {
    const RcCommonSettings& s = cfg.common;
    String j = "{\"type\":\"crsf-router\"";
    j += ",\"ssid\":\"" + webJsonEscape(s.ssid) + "\"";
    j += ",\"pass\":\"" + webJsonEscape(s.pass) + "\"";
    j += ",\"ip\":\"" + webJsonEscape(s.ip) + "\"";
    j += ",\"devName\":\"" + webJsonEscape(s.devName) + "\"";
    j += ",\"moduleAddr\":" + String(s.moduleAddr);
    j += ",\"wifiAuto\":" + String(s.wifiAuto ? "true" : "false");
    j += ",\"wifiTimeout\":" + String(s.wifiTimeout);
    j += ",\"wifiAlways\":" + String(s.wifiAlways ? "true" : "false");
    j += ",\"portb_role\":" + String(s.role[0]);
    j += ",\"portb_baud\":" + String(s.baud[0]);
    j += ",\"portc_role\":" + String(s.role[1]);
    j += ",\"portc_baud\":" + String(s.baud[1]);
    j += ",\"voltMode\":" + String(cfg.voltMode);
    j += ",\"voltCorr\":" + String(cfg.voltCorr);
    j += ",\"tempSend\":" + String(cfg.tempSend ? "true" : "false");
    j += ",\"sportId0\":" + String(cfg.sportPollId[0]);
    j += ",\"sportId1\":" + String(cfg.sportPollId[1]);
    j += "}";
    return j;
}

static void copyStr(char* dst, size_t n, const String& v) {
    strncpy(dst, v.c_str(), n - 1);
    dst[n - 1] = '\0';
}

// Nur Schluessel, die im Body stehen, werden uebernommen (Teil-Updates aus
// den einzelnen Karten der Seite und kompletter Import gehen beide).
static void handleConfigPost() {
    String b = server.arg("plain");
    if (b.length() == 0 || b.length() > 4096) { sendError("bad_data"); return; }
    RcCommonSettings& s = cfg.common;
    int i; bool f; String t;
    if (webJsonString(b, "ssid", t) && t.length())    copyStr(s.ssid, sizeof(s.ssid), t);
    if (webJsonString(b, "pass", t) && t.length() >= 8) copyStr(s.pass, sizeof(s.pass), t);
    if (webJsonString(b, "ip", t) && t.length())      copyStr(s.ip, sizeof(s.ip), t);
    if (webJsonString(b, "devName", t) && t.length()) copyStr(s.devName, sizeof(s.devName), t);
    if (webJsonInt(b, "moduleAddr", i))  s.moduleAddr = i;
    if (webJsonBool(b, "wifiAuto", f))   s.wifiAuto = f;
    if (webJsonInt(b, "wifiTimeout", i)) s.wifiTimeout = (uint16_t)i;
    if (webJsonBool(b, "wifiAlways", f)) s.wifiAlways = f;
    if (webJsonInt(b, "portb_role", i))  s.role[0] = uartRoleClamp(i);
    if (webJsonInt(b, "portb_baud", i))  s.baud[0] = (uint32_t)i;
    if (webJsonInt(b, "portc_role", i))  s.role[1] = uartRoleClamp(i);
    if (webJsonInt(b, "portc_baud", i))  s.baud[1] = (uint32_t)i;
    if (webJsonInt(b, "voltMode", i))    cfg.voltMode = (uint8_t)i;
    if (webJsonInt(b, "voltCorr", i))    cfg.voltCorr = (int8_t)i;
    if (webJsonBool(b, "tempSend", f))   cfg.tempSend = f;
    if (webJsonInt(b, "sportId0", i))    cfg.sportPollId[0] = (uint8_t)i;
    if (webJsonInt(b, "sportId1", i))    cfg.sportPollId[1] = (uint8_t)i;
    cfgSave();
    sendJson("{\"ok\":true}");
}

void webBegin() {
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", WEBUI_HTML); });
    server.on("/api/status", HTTP_GET, []() { sendJson(routerStatusJson()); });
    server.on("/api/config", HTTP_GET, []() { sendJson(configJson()); });
    server.on("/api/config", HTTP_POST, handleConfigPost);
    server.on("/api/restart", HTTP_POST, []() { sendJson("{\"ok\":true}"); routerRequestRestart(); });
    webCommonRegister(server, cfgSave);
    cfgBackupRegister(server);
    server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
}

void webEnableAP() {
    if (apActive) return;
    WiFi.softAP(cfg.common.ssid, cfg.common.pass);
    IPAddress ip, mask(255, 255, 255, 0);
    if (!ip.fromString(cfg.common.ip)) ip.fromString("192.168.1.1");
    WiFi.softAPConfig(ip, ip, mask);
    server.begin();
    apActive = true;
    Serial.printf("WLAN an: \"%s\", http://%s\n", cfg.common.ssid, WiFi.softAPIP().toString().c_str());
}

void webDisableAP() {
    if (!apActive) return;
    server.stop();
    WiFi.softAPdisconnect(true);
    apActive = false;
    Serial.println("WLAN aus");
}

bool webApActive() { return apActive; }

void webLoop() {
    if (apActive) server.handleClient();
}
