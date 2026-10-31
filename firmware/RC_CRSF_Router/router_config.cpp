// router_config.cpp - CRSF-Router, siehe router_config.h
#include "router_config.h"
#include <Preferences.h>

static const char* NVS_NS = "router";

RouterConfig  cfg;
bool          cfgDirty   = false;
unsigned long cfgDirtyMs = 0;

void cfgDefaults() {
    memset(&cfg, 0, sizeof(cfg));
    RcCommonSettings& s = cfg.common;
    strncpy(s.ssid,    "CRSF-Router", sizeof(s.ssid) - 1);
    strncpy(s.pass,    "123456789",   sizeof(s.pass) - 1);
    strncpy(s.ip,      "192.168.1.1", sizeof(s.ip) - 1);
    strncpy(s.devName, "CRSF-Router", sizeof(s.devName) - 1);
    s.moduleAddr  = 15;            // 0xCF, kollidiert nicht mit Multiswitch-Vorgaben 0/1
    s.wifiAuto    = true;          // WLAN nach Timeout ohne RC-Signal
    s.wifiTimeout = 60;
    s.wifiAlways  = false;         // WLAN nicht dauerhaft neben dem Empfaenger funken
    s.role[0]     = UROLE_CRSF;
    s.role[1]     = UROLE_CRSF;
    s.baud[0]     = uartRoleDefaultBaud(UROLE_CRSF);
    s.baud[1]     = uartRoleDefaultBaud(UROLE_CRSF);
    cfg.voltMode       = VOLT_OFF; // erst einschalten, wenn der Spannungsteiler bestueckt ist
    cfg.voltCorr       = 0;
    cfg.tempSend       = false;
    cfg.sportPollId[0] = 0xA1;     // Physical ID 1 (Werkseinstellung FLVSS)
    cfg.sportPollId[1] = 0x22;     // Physical ID 2
}

void cfgSanitize() {
    rcSettingsSanitize(cfg.common);
    if (cfg.common.moduleAddr < 0 || cfg.common.moduleAddr > 15) cfg.common.moduleAddr = 15;
    if (cfg.voltMode > VOLT_CELLS) cfg.voltMode = VOLT_OFF;
    if (cfg.voltCorr < -5) cfg.voltCorr = -5;
    if (cfg.voltCorr > 5)  cfg.voltCorr = 5;
}

void cfgLoad() {
    cfgDefaults();
    Preferences p;
    p.begin(NVS_NS, true);
    rcSettingsLoad(p, cfg.common, nullptr);
    cfg.voltMode       = (uint8_t)p.getInt("vmode", cfg.voltMode);
    cfg.voltCorr       = (int8_t)p.getInt("vcorr", cfg.voltCorr);
    cfg.tempSend       = p.getInt("tsend", cfg.tempSend ? 1 : 0) != 0;
    cfg.sportPollId[0] = (uint8_t)p.getInt("spid0", cfg.sportPollId[0]);
    cfg.sportPollId[1] = (uint8_t)p.getInt("spid1", cfg.sportPollId[1]);
    p.end();
    cfgSanitize();
}

void cfgSave() {
    cfgSanitize();
    Preferences p;
    p.begin(NVS_NS, false);
    rcSettingsSave(p, cfg.common);
    p.putInt("vmode", cfg.voltMode);
    p.putInt("vcorr", cfg.voltCorr);
    p.putInt("tsend", cfg.tempSend ? 1 : 0);
    p.putInt("spid0", cfg.sportPollId[0]);
    p.putInt("spid1", cfg.sportPollId[1]);
    p.end();
    cfgDirty = false;
}

void cfgMarkDirty() {
    cfgDirty   = true;
    cfgDirtyMs = millis();
}
