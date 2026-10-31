// uart_ports.cpp - RC_Common, siehe uart_ports.h
#ifdef ARDUINO
#include "uart_ports.h"
#include "CRSF_ESP32/crsf_esp32.h"
#include "sport_lipo.h"
#include "gps_speed.h"

void uartPortsBegin(UartPort* ports, uint8_t count, CRSF& crsf, bool crsfBus) {
    if (count > 2) count = 2;
    for (uint8_t i = 0; i < count; i++) {
        UartPort& p = ports[i];
        p.duplicate = false;
        p.idle = false;
        p.activeRole = p.ser ? uartRoleClamp(p.role) : UROLE_OFF;
        if (i == 1) p.activeRole = uartRoleResolveSecond(ports[0].activeRole, p.activeRole, &p.duplicate);
        if (p.activeRole == UROLE_CRSF && !crsfBus) { p.idle = true; p.activeRole = UROLE_OFF; }
        if (p.baud == 0) p.baud = uartRoleDefaultBaud(p.activeRole);
        // "Auto": GPS sucht selbst, die anderen Rollen nehmen ihre Protokoll-Baudrate
        if (p.baud == UART_BAUD_AUTO && p.activeRole != UROLE_GPS) p.baud = uartBaudStart(p.activeRole, p.baud);
    }

    // CRSF-Ports gemeinsam an den Router
    HardwareSerial* cs[2] = {nullptr, nullptr};
    uint8_t crx[2] = {0, 0}, ctx[2] = {0, 0};
    uint32_t cb[2] = {BAUD_RATE, BAUD_RATE};
    bool anyCrsf = false;
    for (uint8_t i = 0; i < count; i++) {
        if (ports[i].activeRole != UROLE_CRSF) continue;
        cs[i] = ports[i].ser; crx[i] = ports[i].rx; ctx[i] = ports[i].tx; cb[i] = ports[i].baud;
        anyCrsf = true;
    }
    if (anyCrsf) crsf.init_router(cs[0], crx[0], ctx[0], cs[1], crx[1], ctx[1], cb[0], cb[1]);

    for (uint8_t i = 0; i < count; i++) {
        UartPort& p = ports[i];
        switch (p.activeRole) {
            case UROLE_SPORT: sportLipoInit(p.ser, p.rx, p.tx, p.baud); break;
            case UROLE_GPS:   gpsSpeedInit(p.ser, p.rx, p.tx, p.baud); break;
            case UROLE_RAW:   p.ser->begin(p.baud, SERIAL_8N1, p.rx, p.tx); break;
            default: break;
        }
        if (!p.ser) continue;
        Serial.printf("%s: %s", p.name, uartRoleName(p.activeRole));
        if (p.activeRole != UROLE_OFF) {
            if (p.baud == UART_BAUD_AUTO) Serial.printf(" (RX=%d TX=%d, Baudrate automatisch)", p.rx, p.tx);
            else Serial.printf(" (RX=%d TX=%d, %u Baud)", p.rx, p.tx, (unsigned)p.baud);
        }
        if (p.idle)      Serial.print(" - CRSF nur bei RC-System CRSF aktiv");
        if (p.duplicate) Serial.printf(" - %s laeuft schon auf %s", uartRoleName(p.role), ports[0].name);
        Serial.println();
    }
}

void uartPortsUpdate() {
    sportLipoUpdate();
    gpsSpeedUpdate();
}

void uartPortsTelemetry(CRSF& crsf) {
    static unsigned long last = 0;
    unsigned long now = millis();
    if (now - last < 100) return;
    last = now;
    if (sportLipoIsActive()) {
        if (lipoSensor[0].online) sportSendCellsTelemetry(crsf, 0);
        if (lipoSensor[1].online) sportSendCellsTelemetry(crsf, 1);
    }
    if (gpsIsActive()) {
        uint16_t gspd = (uint16_t)(gpsGetSpeedKmh() * 10.0f);   // CRSF: km/h * 10
        crsf.send_tele_GPS(gpsGetLatE7(), gpsGetLonE7(), gspd,
                           gpsGetHeadingCdeg(), gpsGetAltMeters(), gpsGetSatellites());
    }
}
#endif
