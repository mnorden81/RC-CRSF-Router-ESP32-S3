// settings_common.h - RC_Common: gemeinsame Einstellungen im NVS
//
// Die Funktionen, die alle Module haben, liegen unter denselben Schluesseln
// im NVS-Namensraum des jeweiligen Projekts:
//   ssid, pass, wip, dnam        WLAN-Name, Passwort, AP-IP, Geraetename
//   madr                         Modul-Adresse (WM-Multiswitch-Adresse)
//   wfauto, wftmo, wfalways      WLAN-Auto-Failsafe, Timeout (s), dauerhaft an
//   u1role, u1baud               Zusatz-UART 1 (Port/UART B): Rolle, Baudrate
//   u2role, u2baud               Zusatz-UART 2 (Port/UART C): Rolle, Baudrate
//   rcc                          Version dieses Schemas (1)
// Alle Zahlen als Int (putInt). Fehlt "rcc", werden einmalig die alten
// Schluessel der Projekte gelesen (Multiswitch: ip, mod_adr, wauto, watout,
// walways, pbrole, pcrole, gpsbaud; Soundmodul: pbmode, pbbaud, pcmode,
// pcbaud, pcgps). Die alten Schluessel bleiben stehen, damit eine aeltere
// Firmware nach einem Rueckspielen ihre Einstellungen noch findet.
#ifndef RC_SETTINGS_COMMON_H
#define RC_SETTINGS_COMMON_H

#include <stdint.h>

struct RcCommonSettings {
    char     ssid[33];
    char     pass[65];
    char     ip[16];
    char     devName[24];
    int      moduleAddr;
    bool     wifiAuto;
    uint16_t wifiTimeout;   // 5-240 s
    bool     wifiAlways;
    uint8_t  role[2];       // UROLE_*, [0] = Port/UART B, [1] = Port/UART C
    uint32_t baud[2];
};

// Alte Rollennummer eines Projekts -> UROLE_* (siehe uart_roles.h)
typedef uint8_t (*RcLegacyRoleMap)(int legacy);

static constexpr int RC_SETTINGS_SCHEMA = 1;

#ifdef ARDUINO
#include <Preferences.h>
// s vorher mit den Werkseinstellungen fuellen; fehlende Schluessel lassen
// den Wert unveraendert. legacyPortMode: Umrechnung fuer die alten
// Soundmodul-Schluessel pbmode/pcmode (nullptr = Soundmodul Mini).
// Rueckgabe true = Werte kamen aus alten Schluesseln (bitte speichern).
bool rcSettingsLoad(Preferences& p, RcCommonSettings& s, RcLegacyRoleMap legacyPortMode);
void rcSettingsSave(Preferences& p, const RcCommonSettings& s);
#endif

// Werte in gueltige Bereiche bringen (auch fuer Import aus Web/JSON)
void rcSettingsSanitize(RcCommonSettings& s);

#endif
