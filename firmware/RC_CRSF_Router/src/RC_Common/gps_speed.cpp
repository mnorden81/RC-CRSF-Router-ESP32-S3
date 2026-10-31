/*
 * gps_speed.cpp  –  RC_Common (aus ESP32-RC-Sound v7)
 *
 * Siehe gps_speed.h. Liest ein NMEA-GPS über einen UART (Rolle "GPS")
 * und wertet es mit TinyGPS++ aus. Die Ground Speed kommt direkt
 * aus dem GPS und muss auf dem Modul nicht berechnet werden.
 *
 * Baudrate fest: kurz nach dem Boot wird das GPS einmalig auf 5 Hz (bei
 * 9600 Baud) bzw. 10 Hz (ab 38400) und GGA+RMC gestellt.
 * Baudrate "Auto" (UART_BAUD_AUTO): gps_autobaud sucht die Rate, stellt das
 * GPS auf 115200 (SoftwareSerial: 38400) und 10 Hz und sucht neu, wenn
 * keine Saetze mehr kommen. Befehle gehen jeweils fuer CASIC (ATGM336H)
 * und u-blox raus, die Einstellung ist fluechtig.
 */
#ifdef ARDUINO   // nur Firmware, nicht in den Host-Tests
#include "gps_speed.h"
#include "gps_autobaud.h"
#include "uart_roles.h"
#include <TinyGPS++.h>

// ── Konfiguration ──────────────────────────────────────────────────────────
#define GPS_CFG_DELAY  800    // ms nach Init warten, bevor die Konfig gesendet wird (feste Baudrate)

// ── interner Zustand ───────────────────────────────────────────────────────
// UART (oder SoftwareSerial) kommt vom Projekt, siehe uart_ports.cpp.
static Stream*         gpsSerial = nullptr;
static HardwareSerial* gpsHw     = nullptr;
static GpsBaudFn       gpsSetBaud = nullptr;   // nullptr = Baudrate fest
static TinyGPSPlus     gps;
static GpsAutobaud     autobaud;
static uint32_t        gpsBaud   = 9600;

static float    speedKmh    = 0.0f;
static uint8_t  sats        = 0;
static bool     fixValid    = false;
static int32_t  latE7       = 0;
static int32_t  lonE7       = 0;
static uint16_t headingCdeg = 0;
static uint16_t altMeters   = 0;

static unsigned long initMs  = 0;
static bool          cfgSent = false;

// ── Befehle senden ─────────────────────────────────────────────────────────
static void sendBytes(const uint8_t* b, uint16_t n) {
    if (n) gpsSerial->write(b, n);
}

static void sendNmea(const char* body) {
    char line[48];
    sendBytes((const uint8_t*)line, gpsCmdCasicNmea(body, line, sizeof(line)));
}

// GPS auf eine hoehere Baudrate stellen (gesendet mit der alten Rate)
static void gpsSendBaud(uint32_t baud) {
    uint8_t buf[32];
    sendBytes(buf, gpsCmdCasicBaud(baud, (char*)buf, sizeof(buf)));
    sendBytes(buf, gpsCmdUbxBaud(baud, buf, sizeof(buf)));
    sendBytes(buf, gpsCmdUbxBaudM10(baud, buf, sizeof(buf)));
    gpsSerial->flush();
    delay(60);   // GPS stellt erst nach dem letzten Byte um
}

// Messrate und Saetze passend zur Baudrate (CASIC + u-blox)
static void gpsSendRate(uint32_t baud) {
    uint16_t ms = gpsRateForBaud(baud);
    char body[24];
    snprintf(body, sizeof(body), "PCAS02,%u", (unsigned)ms);
    sendNmea(body);
    // Nur GGA + RMC (GGA=Position/Hoehe/Sats, RMC=Speed/Kurs)
    sendNmea("PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0");
    uint8_t buf[24];
    sendBytes(buf, gpsCmdUbxRate(ms, buf, sizeof(buf)));
    sendBytes(buf, gpsCmdUbxRateM10(ms, buf, sizeof(buf)));
    static const uint8_t off[] = {0x01, 0x02, 0x03, 0x05};   // GLL, GSA, GSV, VTG
    for (uint8_t id : off) sendBytes(buf, gpsCmdUbxMsgOff(id, buf, sizeof(buf)));
    Serial.printf("[GPS] Konfig gesendet: %u Baud, %u ms (%u Hz), GGA+RMC\n",
                  (unsigned)baud, (unsigned)ms, (unsigned)(1000 / ms));
}

static void hwSetBaud(uint32_t baud) {
    gpsHw->flush();
    gpsHw->updateBaudRate(baud);
}

static void applyBaud(uint32_t baud) {
    if (baud != gpsBaud) { gpsSetBaud(baud); gpsBaud = baud; }
    while (gpsSerial->available() > 0) gpsSerial->read();   // Reste der alten Rate
}

static void startCommon(uint32_t baud, uint32_t maxBaud) {
    initMs  = millis();
    cfgSent = false;
    if (baud == UART_BAUD_AUTO && gpsSetBaud) {
        autobaud.begin(initMs, maxBaud);
        gpsBaud = autobaud.baud();
        Serial.printf("[GPS] Baudrate automatisch (bis %u)\n", (unsigned)maxBaud);
    } else {
        gpsSetBaud = nullptr;   // feste Rate
    }
}

// ── Initialisierung ────────────────────────────────────────────────────────
void gpsSpeedInit(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin, uint32_t baud) {
    // RX liest NMEA, TX sendet die Konfiguration an das GPS.
    uint32_t start = uartBaudStart(UROLE_GPS, baud);
    serial->begin(start, SERIAL_8N1, rxPin, txPin);
    gpsSerial  = serial;
    gpsHw      = serial;
    gpsSetBaud = hwSetBaud;
    gpsBaud    = start;
    Serial.printf("[GPS] Init: RX=GPIO%d TX=GPIO%d  %u Baud\n", rxPin, txPin, (unsigned)start);
    startCommon(baud, 115200);
}

void gpsSpeedInitStream(Stream* stream, uint32_t baud, GpsBaudFn setBaud, uint32_t maxBaud) {
    gpsSerial  = stream;
    gpsHw      = nullptr;
    gpsSetBaud = setBaud;
    gpsBaud    = uartBaudStart(UROLE_GPS, baud);
    startCommon(baud, maxBaud);
}

bool     gpsIsActive()   { return gpsSerial != nullptr; }
bool     gpsBaudAuto()   { return gpsSetBaud != nullptr; }
bool     gpsBaudLocked() { return gpsSetBaud ? autobaud.locked() : true; }
uint32_t gpsGetBaud()    { return gpsBaud; }

// ── zyklisches Einlesen/Auswerten ──────────────────────────────────────────
void gpsSpeedUpdate() {
    if (!gpsSerial) return;

    if (gpsSetBaud) {
        // Automatisch: Zeichen gehen an TinyGPS++ und an die Suche
        while (gpsSerial->available() > 0) {
            uint8_t c = (uint8_t)gpsSerial->read();
            autobaud.feed(c);
            gps.encode((char)c);
        }
        uint8_t a = autobaud.poll(millis());
        if (a == GPSB_SWITCH) {
            Serial.printf("[GPS] gefunden bei %u Baud, stelle auf %u um\n",
                          (unsigned)gpsBaud, (unsigned)autobaud.target());
            gpsSendBaud(autobaud.target());
        }
        if (a != GPSB_IDLE) applyBaud(autobaud.baud());
        if (a == GPSB_LOCKED) {
            Serial.printf("[GPS] Baudrate %u%s\n", (unsigned)gpsBaud,
                          autobaud.switchFailed() ? " (Umstellen nicht moeglich)" : "");
            gpsSendRate(gpsBaud);
        }
        if (a == GPSB_LOST) Serial.println("[GPS] keine Daten, suche Baudrate neu");
    } else {
        // Feste Baudrate: Konfiguration einmalig kurz nach dem Boot senden (GPS
        // ist dann bereit, ein evtl. Boot-Glitch auf dem TX-Pin ist vorbei)
        if (!cfgSent && (millis() - initMs) > GPS_CFG_DELAY) {
            gpsSendRate(gpsBaud);
            cfgSent = true;
        }
        while (gpsSerial->available() > 0) {
            gps.encode((char)gpsSerial->read());
        }
    }

    if (gps.satellites.isValid()) sats = (uint8_t)gps.satellites.value();
    fixValid = gps.location.isValid();

    if (gps.speed.isValid()) {
        speedKmh = (float)gps.speed.kmph();
        if (speedKmh < 0.0f) speedKmh = 0.0f;
    }
    if (gps.location.isValid()) {
        latE7 = (int32_t)(gps.location.lat() * 1e7);
        lonE7 = (int32_t)(gps.location.lng() * 1e7);
    }
    if (gps.course.isValid()) {
        headingCdeg = (uint16_t)(gps.course.deg() * 100.0);
    }
    if (gps.altitude.isValid()) {
        double a = gps.altitude.meters();
        if (a < 0) a = 0;
        altMeters = (uint16_t)a;
    }
}

// ── Abfragefunktionen ──────────────────────────────────────────────────────
float    gpsGetSpeedKmh()    { return speedKmh; }
uint8_t  gpsGetSatellites()  { return sats; }
bool     gpsHasFix()         { return fixValid; }
int32_t  gpsGetLatE7()       { return latE7; }
int32_t  gpsGetLonE7()       { return lonE7; }
uint16_t gpsGetHeadingCdeg() { return headingCdeg; }
uint16_t gpsGetAltMeters()   { return altMeters; }

#endif // ARDUINO
