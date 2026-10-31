// settings_common.cpp - RC_Common, siehe settings_common.h
#include "settings_common.h"
#include "uart_roles.h"
#include <string.h>

void rcSettingsSanitize(RcCommonSettings& s) {
    s.ssid[sizeof(s.ssid) - 1] = '\0';
    s.pass[sizeof(s.pass) - 1] = '\0';
    s.ip[sizeof(s.ip) - 1] = '\0';
    s.devName[sizeof(s.devName) - 1] = '\0';
    if (s.moduleAddr < 0) s.moduleAddr = 0;
    if (s.moduleAddr > 255) s.moduleAddr = 255;
    if (s.wifiTimeout < 5 || s.wifiTimeout > 240) s.wifiTimeout = 60;
    for (int i = 0; i < 2; i++) {
        s.role[i] = uartRoleClamp(s.role[i]);
        if (!uartBaudValid(s.baud[i])) s.baud[i] = uartRoleDefaultBaud(s.role[i]);
        if (s.baud[i] == 0) s.baud[i] = 115200;
    }
}

#ifdef ARDUINO
static void loadStr(Preferences& p, const char* key, char* dst, size_t len) {
    if (!p.isKey(key)) return;
    String v = p.getString(key, dst);
    strncpy(dst, v.c_str(), len - 1);
    dst[len - 1] = '\0';
}

bool rcSettingsLoad(Preferences& p, RcCommonSettings& s, RcLegacyRoleMap legacyPortMode) {
    loadStr(p, "ssid", s.ssid, sizeof(s.ssid));
    loadStr(p, "pass", s.pass, sizeof(s.pass));
    loadStr(p, "dnam", s.devName, sizeof(s.devName));

    if (p.isKey("rcc")) {
        loadStr(p, "wip", s.ip, sizeof(s.ip));
        s.moduleAddr  = p.getInt("madr", s.moduleAddr);
        s.wifiAuto    = p.getInt("wfauto", s.wifiAuto) != 0;
        s.wifiTimeout = (uint16_t)p.getInt("wftmo", s.wifiTimeout);
        s.wifiAlways  = p.getInt("wfalways", s.wifiAlways) != 0;
        s.role[0] = uartRoleClamp(p.getInt("u1role", s.role[0]));
        s.baud[0] = (uint32_t)p.getInt("u1baud", (int)s.baud[0]);
        s.role[1] = uartRoleClamp(p.getInt("u2role", s.role[1]));
        s.baud[1] = (uint32_t)p.getInt("u2baud", (int)s.baud[1]);
        rcSettingsSanitize(s);
        return false;
    }

    // ── Einmalige Uebernahme der alten Schluessel ─────────────────────────
    if (p.isKey("wip")) loadStr(p, "wip", s.ip, sizeof(s.ip));
    else                loadStr(p, "ip",  s.ip, sizeof(s.ip));        // Multiswitch
    if (p.isKey("madr"))         s.moduleAddr = p.getInt("madr", s.moduleAddr);
    else if (p.isKey("mod_adr")) s.moduleAddr = p.getInt("mod_adr", s.moduleAddr);

    if (p.isKey("wfauto"))      s.wifiAuto = p.getInt("wfauto", s.wifiAuto) != 0;
    else if (p.isKey("wauto"))  s.wifiAuto = p.getBool("wauto", s.wifiAuto);
    if (p.isKey("wftmo"))       s.wifiTimeout = (uint16_t)p.getInt("wftmo", s.wifiTimeout);
    else if (p.isKey("watout")) s.wifiTimeout = (uint16_t)p.getUInt("watout", s.wifiTimeout);
    if (p.isKey("wfalways"))     s.wifiAlways = p.getInt("wfalways", s.wifiAlways) != 0;
    else if (p.isKey("walways")) s.wifiAlways = p.getBool("walways", s.wifiAlways);

    RcLegacyRoleMap soundMap = legacyPortMode ? legacyPortMode : uartRoleFromSoundMiniLegacy;
    bool b = false, c = false;
    if (p.isKey("pbrole")) { s.role[0] = uartRoleFromMultiswitchLegacy(p.getInt("pbrole", 0)); b = true; }
    else if (p.isKey("pbmode")) { s.role[0] = soundMap(p.getInt("pbmode", 0)); b = true; }
    if (p.isKey("pcrole")) { s.role[1] = uartRoleFromMultiswitchLegacy(p.getInt("pcrole", 0)); c = true; }
    else if (p.isKey("pcmode")) { s.role[1] = soundMap(p.getInt("pcmode", 0)); c = true; }
    else if (p.isKey("pcgps"))  { s.role[1] = p.getInt("pcgps", 0) ? UROLE_GPS : UROLE_OFF; c = true; }

    // Baudraten: Soundmodul hatte sie je Port, der Multiswitch nur fuer GPS.
    // GPS war bisher fest eingestellt und sucht ab RC_Common 1.1.0 selbst.
    if (p.isKey("pbbaud")) s.baud[0] = (uint32_t)p.getInt("pbbaud", (int)s.baud[0]);
    else if (b) s.baud[0] = uartRoleDefaultBaud(s.role[0]);
    if (p.isKey("pcbaud")) s.baud[1] = (uint32_t)p.getInt("pcbaud", (int)s.baud[1]);
    else if (c) s.baud[1] = uartRoleDefaultBaud(s.role[1]);
    for (int i = 0; i < 2; i++) if (s.role[i] == UROLE_GPS) s.baud[i] = UART_BAUD_AUTO;

    rcSettingsSanitize(s);
    return true;
}

void rcSettingsSave(Preferences& p, const RcCommonSettings& s) {
    p.putString("ssid", s.ssid);
    p.putString("pass", s.pass);
    p.putString("wip",  s.ip);
    p.putString("dnam", s.devName);
    p.putInt("madr",     s.moduleAddr);
    p.putInt("wfauto",   s.wifiAuto ? 1 : 0);
    p.putInt("wftmo",    s.wifiTimeout);
    p.putInt("wfalways", s.wifiAlways ? 1 : 0);
    p.putInt("u1role",   s.role[0]);
    p.putInt("u1baud",   (int)s.baud[0]);
    p.putInt("u2role",   s.role[1]);
    p.putInt("u2baud",   (int)s.baud[1]);
    p.putInt("rcc",      RC_SETTINGS_SCHEMA);
}
#endif
