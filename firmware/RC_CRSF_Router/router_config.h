#ifndef ROUTER_CONFIG_H
#define ROUTER_CONFIG_H

/*
 * router_config.h  -  CRSF-Router: Einstellungen im NVS (Namensraum "router")
 *
 * Gemeinsame Werte (WLAN, Geraetename, Moduladresse, Rollen und Baudraten
 * von Port B/C) liegen unter den RC_Common-Schluesseln (settings_common.h),
 * die Router-eigenen Werte unter eigenen Schluesseln.
 */

#include <Arduino.h>
#include "src/RC_Common/settings_common.h"
#include "src/RC_Common/uart_roles.h"

// Spannung senden: 0 = Aus, 1 = als RxBt (Batterie-Frame 0x08),
// 2 = als Zellen-Wert (Frame 0x0E, Quelle 8)
static constexpr uint8_t VOLT_OFF   = 0;
static constexpr uint8_t VOLT_RXBT  = 1;
static constexpr uint8_t VOLT_CELLS = 2;

struct RouterConfig {
    RcCommonSettings common;   // ssid, pass, ip, devName, moduleAddr, WLAN, role[2], baud[2]
    uint8_t voltMode;          // VOLT_*
    int8_t  voltCorr;          // Korrektur in 0,1 V, -5..+5
    bool    tempSend;          // Temperatur als CRSF-Telemetrie (0x0D)
    uint8_t sportPollId[2];    // S.Port Poll-Bytes Sensor 1/2
};

extern RouterConfig  cfg;
extern bool          cfgDirty;
extern unsigned long cfgDirtyMs;

void cfgDefaults();
void cfgLoad();
void cfgSave();
void cfgMarkDirty();
void cfgSanitize();

#endif
