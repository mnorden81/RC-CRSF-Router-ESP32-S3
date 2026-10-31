/*
   RC_CRSF_Router - CRSF-Verteiler fuer ELRS-Empfaenger (ESP32-S3-Zero)
   PiperPilot

   Der Router haengt an einem freien CRSF-Port des Empfaengers (Uplink) und
   verteilt CRSF auf Port B und Port C. An jedem Port: weiteres Modul,
   weiterer Router (Kette) oder Sensor. Die Weiterleitung kommt aus
   RC_Common (CRSF_ESP32 1.1.0, gleiche Basis wie Multiswitch und
   Soundmodule):
     - vom Empfaenger nach unten: alles ausser Anfragen an die eigene Adresse
     - von unten nach oben: alles
     - zwischen Port B und Port C: nie (keine Schleifen)
     - nur vollstaendige Frames mit gueltiger CRC, unveraendert

   ── Ports (pins.h) ────────────────────────────────────────────────────
   Uplink  = CRSF zum Empfaenger, Serial2, RX 5 / TX 6, fest
   Port B  = Serial1, RX 7 / TX 8      Rolle Aus/CRSF/S.Port/GPS/Raw
   Port C  = Serial0, RX 44 / TX 43    Rolle wie Port B
   Rollen und Baudraten gelten nach einem Neustart. S.Port, GPS und Raw
   nur auf einem Port (doppelt gewaehlt laeuft die Rolle auf Port B).

   ── Adresse ───────────────────────────────────────────────────────────
   Eigene WM-Moduladresse 0-15 -> CRSF-Adresse 0xC0+Adresse, Vorgabe 15
   (0xCF). Mehrere Router im Modell bekommen verschiedene Adressen (z. B.
   15, 14, 13). Meldet ein Geraet weiter hinten dieselbe Adresse, blinkt
   die LED gelb (Doppelblitz).

   ── Status-LED (WS2812, GPIO21) ───────────────────────────────────────
   gruen langsam      RC-Signal in Ordnung
   violett langsam    RC-Signal in Ordnung, WLAN an
   rot schnell        kein RC-Signal / Failsafe
   gelb Doppelblitz   Adresskonflikt oder doppelte Port-Rolle
   blau blinkend      3 s nach dem Start: BOOT druecken = WLAN an

   ── Telemetrie ────────────────────────────────────────────────────────
   Empfaengerspannung (Spannungsteiler an GPIO1) als RxBt oder Zellen-Wert,
   Temperatur des ESP32 (0x0D), dazu S.Port-FLVSS und GPS an Port B/C.

   ── WLAN ──────────────────────────────────────────────────────────────
   Access Point "CRSF-Router" (192.168.1.1) ueber BOOT-Taste, Lua "WLAN
   jetzt", Auto-WLAN nach Timeout ohne RC-Signal oder "WLAN dauerhaft an".

   ── Arduino IDE ───────────────────────────────────────────────────────
   Board "ESP32S3 Dev Module", USB CDC On Boot: Enabled, Flash 4 MB,
   Partition Scheme "Default 4MB with spiffs", Core esp32 3.0.7,
   Bibliothek TinyGPSPlus 1.0.3.
*/

#include <Arduino.h>
#include "src/RC_Common/rc_common.h"
#include "src/RC_Common/CRSF_ESP32/crsf_esp32.h"
#include "src/RC_Common/uart_ports.h"
#include "src/RC_Common/sport_lipo.h"
#include "src/RC_Common/gps_speed.h"
#include "src/RC_Common/lua_menu.h"
#include "src/RC_Common/wifi_failsafe.h"
#include "src/RC_Common/status_led.h"
#include "pins.h"
#include "router_config.h"
#include "router_web.h"

// Version: bei jeder Aenderung hochzaehlen (auch PARAM-Version im DEVICE_INFO)
#define FW_MAJOR 0
#define FW_MINOR 3
#define FW_PATCH 0
static const char*   FW_VERSION       = "0.3.0";
static const uint8_t FW_PARAM_VERSION = FW_MAJOR * 100 + FW_MINOR * 10 + FW_PATCH;   // 30
// v0.3.0: erste Firmware auf RC_Common 1.2.0 (Router, Rollen, Lua, WebUI,
//         Status-LED in Farben, Spannung/Temperatur als Telemetrie).
// v0.2.0: Geruest mit eigenem CRSF-Code (firmware/v0.2.0-geruest, ungetestet).

static HardwareSerial* const UPLINK_SERIAL = &Serial2;
static HardwareSerial* const PORTB_SERIAL  = &Serial1;
static HardwareSerial* const PORTC_SERIAL  = &Serial0;

static UartPort uartPorts[2] = {
    {"Port B", PORTB_SERIAL, PIN_PORTB_RX, PIN_PORTB_TX, UROLE_OFF, 0, UROLE_OFF, false, false},
    {"Port C", PORTC_SERIAL, PIN_PORTC_RX, PIN_PORTC_TX, UROLE_OFF, 0, UROLE_OFF, false, false},
};

CRSF crsf;
static WifiFailsafe wifiFailsafe;

static bool          busOk          = false;
static unsigned long lastFrameMs    = 0;
static unsigned long restartAtMs    = 0;
static float         rxVolt         = 0.0f;   // Empfaengerspannung (V), korrigiert
static float         espTemp        = 0.0f;   // Temperatur ESP32 (Grad C)
static uint8_t       ledMode        = SLED_OFF;
static const char*   ledText        = "";

static constexpr unsigned long CRSF_TIMEOUT_MS = 2000;
#define CRSF_SLOT_MS 5
static inline uint8_t  ownAddr()       { return 0xC0 + (uint8_t)constrain(cfg.common.moduleAddr, 0, 15); }
static inline uint16_t slotDelayMs()   { return (uint16_t)constrain(cfg.common.moduleAddr, 0, 15) * 2 * CRSF_SLOT_MS; }
static bool configError()              { return crsf.isDuplicateAddress() || uartPorts[1].duplicate; }

void routerRequestRestart() { cfgSave(); restartAtMs = millis() + 500; if (!restartAtMs) restartAtMs = 1; }

// ======== Lua-Menue ===================================================
//  0 Root            1 Info Version
//  2 Ordner Ports    3 Port B Rolle  4 Port B Baud  5 Port C Rolle  6 Port C Baud
//  7 Moduladresse
//  8 Ordner Telemetrie  9 Spannung senden  10 Korrektur  11 Info Spannung
//                      12 Temperatur senden  13 Info Temperatur
// 14 Ordner Status  15 Frames runter  16 Frames hoch  17 Verworfen
//                   18 CRC-Fehler unten  19 Adresskonflikt
// 20 Ordner WLAN    21 WLAN jetzt  22 Auto-WLAN  23 Auto-WLAN Timeout  24 WLAN dauerhaft an
// 25 Neustart       26 Info Geraetename
static constexpr uint8_t LUA_COUNT = 26;
static LuaItem luaItems[LUA_COUNT + 1];
static LuaMenu luaMenu(luaItems, sizeof(luaItems) / sizeof(luaItems[0]));

static void luaShowVersion(uint8_t, LuaView& v, char* buf) { snprintf(buf, LUA_VIEW_BUF, "%s S3", FW_VERSION); v.text = buf; }
static void luaShowName(uint8_t, LuaView& v, char* buf)    { snprintf(buf, LUA_VIEW_BUF, "%s", cfg.common.devName); v.text = buf; }

static uint8_t luaGetRole(uint8_t a) { return uartRoleClamp(cfg.common.role[a]); }
static void    luaSetRole(uint8_t a, uint8_t v) {
    if (v != cfg.common.role[a]) {
        uint32_t def = uartRoleDefaultBaud(v);
        if (def != 0 && v != UROLE_RAW) cfg.common.baud[a] = def;
    }
    cfg.common.role[a] = v; cfgMarkDirty();
}
static uint8_t luaGetBaud(uint8_t a)            { return uartBaudToIndex(cfg.common.baud[a]); }
static void    luaSetBaud(uint8_t a, uint8_t v) { cfg.common.baud[a] = uartBaudFromIndex(v); cfgMarkDirty(); }
static uint8_t luaGetAddr(uint8_t)              { return (uint8_t)constrain(cfg.common.moduleAddr, 0, 15); }
static void    luaSetAddr(uint8_t, uint8_t v)   { cfg.common.moduleAddr = v; crsf.setDeviceAddress(ownAddr()); cfgMarkDirty(); }

static uint8_t luaGetVMode(uint8_t)             { return cfg.voltMode; }
static void    luaSetVMode(uint8_t, uint8_t v)  { cfg.voltMode = v; cfgMarkDirty(); }
static uint8_t luaGetVCorr(uint8_t)             { return (uint8_t)(cfg.voltCorr + 5); }
static void    luaSetVCorr(uint8_t, uint8_t v)  { cfg.voltCorr = (int8_t)v - 5; cfgMarkDirty(); }
static uint8_t luaGetTSend(uint8_t)             { return cfg.tempSend ? 1 : 0; }
static void    luaSetTSend(uint8_t, uint8_t v)  { cfg.tempSend = v; cfgMarkDirty(); }
static void luaShowVolt(uint8_t, LuaView& v, char* buf) { snprintf(buf, LUA_VIEW_BUF, "%.2f V", rxVolt); v.text = buf; }
static void luaShowTemp(uint8_t, LuaView& v, char* buf) { snprintf(buf, LUA_VIEW_BUF, "%.1f C", espTemp); v.text = buf; }
static void luaShowCount(uint8_t a, LuaView& v, char* buf) {
    uint32_t n = a == 0 ? crsf.getFwdDown() : a == 1 ? crsf.getFwdUp() : a == 2 ? crsf.getFwdDropped() : crsf.getDownCrcErrors();
    snprintf(buf, LUA_VIEW_BUF, "%lu", (unsigned long)n); v.text = buf;
}
static void luaShowDup(uint8_t, LuaView& v, char*) { v.text = crsf.isDuplicateAddress() ? "ja" : "nein"; }

static uint8_t luaGetWifiNow(uint8_t)             { return webApActive() ? 1 : 0; }
static void    luaSetWifiNow(uint8_t, uint8_t v)  { if (v) webEnableAP(); else webDisableAP(); }
static uint8_t luaGetWifiAuto(uint8_t)            { return cfg.common.wifiAuto ? 1 : 0; }
static void    luaSetWifiAuto(uint8_t, uint8_t v) { cfg.common.wifiAuto = v; cfgMarkDirty(); }
static uint8_t luaGetWifiTmo(uint8_t)             { return (uint8_t)constrain(cfg.common.wifiTimeout, 5, 240); }
static void    luaSetWifiTmo(uint8_t, uint8_t v)  { cfg.common.wifiTimeout = v; cfgMarkDirty(); }
static uint8_t luaGetWifiAlw(uint8_t)             { return cfg.common.wifiAlways ? 1 : 0; }
static void    luaSetWifiAlw(uint8_t, uint8_t v)  { cfg.common.wifiAlways = v; cfgMarkDirty(); }
static uint8_t luaGetRestart(uint8_t)             { return restartAtMs ? 1 : 0; }
static void    luaSetRestart(uint8_t, uint8_t v)  { if (v) routerRequestRestart(); }

static void buildLuaMenu() {
    static const char* AUS_EIN = "Aus;Ein";
    LuaMenu& m = luaMenu;
    m.clear();
    m.folder(0, 0, "");
    m.info(1, 0, "Version", "")->view = luaShowVersion;
    m.folder(2, 0, "Ports (nach Neustart)");
    m.select(3, 2, "Port B Rolle", uartRoleOptions(), luaGetRole, luaSetRole, 0);
    m.select(4, 2, "Port B Baudrate", uartBaudOptions(), luaGetBaud, luaSetBaud, 0);
    m.select(5, 2, "Port C Rolle", uartRoleOptions(), luaGetRole, luaSetRole, 1);
    m.select(6, 2, "Port C Baudrate", uartBaudOptions(), luaGetBaud, luaSetBaud, 1);
    m.u8(7, 0, "Modul Adresse", 0, 15, "", luaGetAddr, luaSetAddr);
    m.folder(8, 0, "Telemetrie");
    m.select(9, 8, "Spannung senden", "Aus;als RxBt;als Zellen", luaGetVMode, luaSetVMode);
    m.select(10, 8, "Spannung Korrektur", "-0.5V;-0.4V;-0.3V;-0.2V;-0.1V;0.0V;+0.1V;+0.2V;+0.3V;+0.4V;+0.5V", luaGetVCorr, luaSetVCorr);
    m.info(11, 8, "Empfaengerspannung", "")->view = luaShowVolt;
    m.select(12, 8, "Temperatur senden", AUS_EIN, luaGetTSend, luaSetTSend);
    m.info(13, 8, "Temperatur", "")->view = luaShowTemp;
    m.folder(14, 0, "Status");
    static const char* CNT[4] = {"Frames runter", "Frames hoch", "Verworfen", "CRC-Fehler unten"};
    for (uint8_t a = 0; a < 4; a++) { LuaItem* it = m.info(15 + a, 14, CNT[a], ""); it->arg = a; it->view = luaShowCount; }
    m.info(19, 14, "Adresskonflikt", "")->view = luaShowDup;
    m.folder(20, 0, "WLAN");
    m.select(21, 20, "WLAN jetzt", AUS_EIN, luaGetWifiNow, luaSetWifiNow);
    m.select(22, 20, "Auto-WLAN ohne Signal", AUS_EIN, luaGetWifiAuto, luaSetWifiAuto);
    m.u8(23, 20, "Auto-WLAN nach", 5, 240, "s", luaGetWifiTmo, luaSetWifiTmo);
    m.select(24, 20, "WLAN dauerhaft an", AUS_EIN, luaGetWifiAlw, luaSetWifiAlw);
    m.select(25, 0, "Neustart", "Nein;Jetzt", luaGetRestart, luaSetRestart);
    m.info(26, 0, "Geraetename", "")->view = luaShowName;
}

// ======== Status-LED (Farben) =========================================
// Muster aus RC_Common (statusLedMode/statusLedOn), Farbe hier im Projekt.
static void updateLed() {
    unsigned long now = millis();
    const LinkStatistics& ls = crsf.getLinkStatistics();
    StatusLedInput in = {};
    in.configError   = configError();
    in.crsf          = true;
    in.busOk         = busOk;
    in.lastRcFrameMs = crsf.getLastRcFrameMs();
    in.linkStatsMs   = ls.lastUpdate;
    in.uplinkLQ      = ls.uplinkLQ;
    uint8_t mode = statusLedMode(in, now);
    if (mode == SLED_OFF) mode = SLED_FAST;   // kein Signal: rot schnell statt dunkel
    static unsigned long start = 0;
    if (mode != ledMode) { ledMode = mode; start = now; }
    uint8_t r = 0, g = 0, b = 0;
    if (mode == SLED_DOUBLE)    { r = 40; g = 28; ledText = "gelb Doppelblitz: Adresskonflikt oder doppelte Port-Rolle"; }
    else if (mode == SLED_FAST) { r = 40;         ledText = "rot schnell: kein RC-Signal"; }
    else if (webApActive())     { r = 24; b = 40; ledText = "violett: RC-Signal OK, WLAN an"; }
    else                        { g = 40;         ledText = "gruen: RC-Signal OK"; }
    bool on = statusLedOn(mode, now - start);
    static int8_t last = -1; static uint32_t lastRgb = 0;
    uint32_t rgb = on ? ((uint32_t)r << 16 | (uint32_t)g << 8 | b) : 0;
    if ((int8_t)on != last || rgb != lastRgb) { neopixelWrite(PIN_STATUS_LED, r * on, g * on, b * on); last = on; lastRgb = rgb; }
}

// BOOT-Taste: 3 s nach dem Start druecken = WLAN an (LED blinkt blau)
static bool waitForWifiButton() {
    bool pressed = false;
    unsigned long start = millis();
    while (millis() - start < WIFI_BUTTON_WINDOW_MS) {
        if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
            delay(30);
            if (digitalRead(PIN_BOOT_BUTTON) == LOW) { pressed = true; break; }
        }
        neopixelWrite(PIN_STATUS_LED, 0, 0, ((millis() - start) / 250) % 2 ? 40 : 0);
        delay(10);
    }
    neopixelWrite(PIN_STATUS_LED, 0, 0, 0);
    return pressed;
}

// ======== Telemetrie ==================================================
static void readSensors() {
    static unsigned long last = 0;
    unsigned long now = millis();
    if (now - last < 200) return;
    last = now;
    float v = analogReadMilliVolts(PIN_VSENSE) / 1000.0f * (VSENSE_R_TOP + VSENSE_R_BOTTOM) / VSENSE_R_BOTTOM;
    v += cfg.voltCorr * 0.1f;
    if (v < 0) v = 0;
    rxVolt  = rxVolt == 0.0f ? v : rxVolt * 0.8f + v * 0.2f;   // leicht glaetten
    espTemp = temperatureRead();
}

static void sendOwnTelemetry() {
    if (!busOk) return;
    static unsigned long lastV = 0, lastT = 0;
    unsigned long now = millis();
    if (cfg.voltMode != VOLT_OFF && now - lastV >= 500) {
        lastV = now;
        if (cfg.voltMode == VOLT_RXBT) crsf.send_tele_Battery_Sensor((uint16_t)(rxVolt * 10.0f + 0.5f), 0, 0, 0);
        else crsf.send_tele_Voltages(8, {(int32_t)(rxVolt * 1000.0f + 0.5f)});   // Quelle 8 (FLVSS nutzt 1/2)
    }
    if (cfg.tempSend && now - lastT >= 2000) {
        lastT = now;
        crsf.send_tele_Temp(0, {(int32_t)(espTemp * 10.0f)});   // 0,1 Grad C
    }
}

// ======== Status fuer die WebUI ========================================
String routerStatusJson() {
    const LinkStatistics& ls = crsf.getLinkStatistics();
    String j = "{\"version\":\"" + String(FW_VERSION) + "\",\"rcCommon\":\"" RC_COMMON_VERSION "\"";
    j += ",\"rc\":" + String(busOk && crsf.getLastRcFrameMs() && millis() - crsf.getLastRcFrameMs() < 1000 ? "true" : "false");
    j += ",\"lq\":" + String(ls.uplinkLQ);
    j += ",\"moduleAddr\":" + String(cfg.common.moduleAddr);
    j += ",\"dupAddr\":" + String(crsf.isDuplicateAddress() ? "true" : "false");
    j += ",\"ledText\":\"" + String(ledText) + "\"";
    j += ",\"volt\":" + String(rxVolt, 2) + ",\"temp\":" + String(espTemp, 1);
    j += ",\"uptime\":" + String(millis() / 1000);
    j += ",\"fwdDown\":" + String(crsf.getFwdDown()) + ",\"fwdUp\":" + String(crsf.getFwdUp());
    j += ",\"dropped\":" + String(crsf.getFwdDropped()) + ",\"crcDown\":" + String(crsf.getDownCrcErrors());
    j += ",\"ports\":[";
    for (uint8_t i = 0; i < 2; i++) {
        if (i) j += ',';
        j += "{\"name\":\"" + String(uartPorts[i].name) + "\",\"role\":" + String(uartPorts[i].activeRole) +
             ",\"baud\":" + String(uartPorts[i].baud) + ",\"duplicate\":" + String(uartPorts[i].duplicate ? "true" : "false") + "}";
    }
    j += "],\"cells\":[";
    if (sportLipoIsActive()) {
        for (uint8_t i = 0; i < SPORT_NUM_SENSORS; i++) {
            if (i) j += ',';
            j += "{\"online\":" + String(lipoSensor[i].online ? "true" : "false") + ",\"cells\":" + String(lipoSensor[i].cellCount) +
                 ",\"total\":" + String(lipoSensor[i].totalVoltage, 2) + "}";
        }
    }
    j += "]";
    if (gpsIsActive()) j += ",\"gps\":{\"sats\":" + String(gpsGetSatellites()) + ",\"kmh\":" + String(gpsGetSpeedKmh(), 1) + "}";
    j += "}";
    return j;
}

// ======== Setup / Loop =================================================
void setup() {
    Serial.begin(115200);
    cfgLoad();
    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

    crsf.init_crsf(UPLINK_SERIAL, PIN_UPLINK_RX, PIN_UPLINK_TX);
    crsf.setDeviceAddress(ownAddr());

    webBegin();
    if (cfg.common.wifiAlways) webEnableAP();
    else if (waitForWifiButton()) webEnableAP();

    sportLipoUsePollIds(cfg.sportPollId);
    uartPorts[0].role = cfg.common.role[0]; uartPorts[0].baud = cfg.common.baud[0];
    uartPorts[1].role = cfg.common.role[1]; uartPorts[1].baud = cfg.common.baud[1];
    uartPortsBegin(uartPorts, 2, crsf, true);

    buildLuaMenu();
    cfgSave();   // fehlende Schluessel beim ersten Start anlegen
    Serial.printf("CRSF-Router %s, RC_Common %s, Adresse 0x%02X, Lua %u Felder\n",
                  FW_VERSION, RC_COMMON_VERSION, ownAddr(), (unsigned)luaMenu.size());
}

void loop() {
    unsigned long now = millis();
    webLoop();
    uartPortsUpdate();

    crsf.read_packets(0);
    crsf.route_packets();
    static uint32_t lastValid = 0;
    uint32_t vf = crsf.getValidFrames();
    if (vf != lastValid) { lastValid = vf; busOk = true; lastFrameMs = now; }
    if (now - lastFrameMs > CRSF_TIMEOUT_MS) busOk = false;

    readSensors();
    if (busOk) uartPortsTelemetry(crsf);
    sendOwnTelemetry();
    updateLed();

    if (crsf.getDeviceInfoReplyPending() && now - crsf.getPingTime() >= slotDelayMs()) {
        crsf.setDeviceInfoReplyPending(false);
        char dn[32];
        snprintf(dn, sizeof(dn), "%s@%d", cfg.common.devName, cfg.common.moduleAddr);
        crsf.send_device_info(dn, luaMenu.maxId(), FW_PARAM_VERSION);
    }
    if (crsf.getDeviceReadReplyPending()) {
        uint8_t id = crsf.getParamReadIndex();
        crsf.setDeviceReadReplyPending(false);
        luaMenu.send(crsf, id);
    }
    if (crsf.getDeviceWriteReplyPending()) {
        uint8_t id = crsf.getParamWriteIndex(), v = crsf.getParamWriteValue();
        crsf.setDeviceWriteReplyPending(false);
        luaMenu.write(id, v);
        luaMenu.send(crsf, id);
    }
    // Schaltbefehle (WM-Protokoll) sind fuer die Module weiter hinten, der
    // Router selbst schaltet nichts.
    if (crsf.getDeviceCommandReplyPending()) crsf.setDeviceCommandReplyPending(false);

    if (cfgDirty && now - cfgDirtyMs >= 500) cfgSave();

    if (wifiFailsafe.check(now, cfg.common.wifiAlways, cfg.common.wifiAuto, cfg.common.wifiTimeout,
                           busOk, webApActive())) {
        Serial.println("Auto-WLAN: kein RC-Signal, WLAN an");
        webEnableAP();
    }

    if (restartAtMs && (long)(now - restartAtMs) >= 0) ESP.restart();
}
