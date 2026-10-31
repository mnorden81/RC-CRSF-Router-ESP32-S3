// gps_autobaud.cpp - RC_Common, siehe gps_autobaud.h
#include "gps_autobaud.h"
#include <stdio.h>
#include <string.h>

const uint32_t GPSB_SCAN_LIST[GPSB_SCAN_COUNT] = {9600, 115200, 38400, 57600, 19200};

// ── NMEA-Pruefsumme ───────────────────────────────────────────────────────
static int hexVal(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool NmeaChecker::feed(uint8_t c) {
    if (c == '$') { state = 1; sum = 0; len = 0; return false; }
    switch (state) {
        case 1:
            if (c == '*') { state = 2; return false; }
            if (c < 0x20 || c > 0x7E || ++len > 90) { state = 0; return false; }
            sum ^= c;
            return false;
        case 2: {
            int v = hexVal(c);
            if (v < 0) { state = 0; return false; }
            got = (uint8_t)(v << 4);
            state = 3;
            return false;
        }
        case 3: {
            int v = hexVal(c);
            state = 0;
            return v >= 0 && (uint8_t)(got | v) == sum && len >= 5;
        }
        default:
            return false;
    }
}

// ── Ablauf ───────────────────────────────────────────────────────────────
void GpsAutobaud::begin(uint32_t now, uint32_t maxBaud) {
    maxB = maxBaud < 9600 ? 9600 : maxBaud;
    tgt = 9600;
    for (uint8_t i = 0; i < GPSB_SCAN_COUNT; i++)
        if (GPSB_SCAN_LIST[i] <= maxB && GPSB_SCAN_LIST[i] > tgt) tgt = GPSB_SCAN_LIST[i];
    idx = 0;
    cur = found = GPSB_SCAN_LIST[0];
    st = ST_SCAN;
    valid = 0;
    since = now;
    failed = false;
    started = false;
    nmea.reset();
}

bool GpsAutobaud::nextScanBaud() {
    for (uint8_t n = 0; n < GPSB_SCAN_COUNT; n++) {
        idx = (uint8_t)((idx + 1) % GPSB_SCAN_COUNT);
        if (GPSB_SCAN_LIST[idx] <= maxB) { cur = GPSB_SCAN_LIST[idx]; return true; }
    }
    return false;
}

void GpsAutobaud::feed(uint8_t c) {
    if (nmea.feed(c) && valid < 255) valid++;
}

uint8_t GpsAutobaud::poll(uint32_t now) {
    if (!started) {
        started = true;
        since = now;
        return GPSB_TRY;
    }
    switch (st) {
        case ST_SCAN:
            if (valid >= GPSB_NEED) {
                found = cur;
                valid = 0;
                since = now;
                nmea.reset();
                if (cur < tgt && !failed) {
                    cur = tgt;
                    st = ST_VERIFY;
                    return GPSB_SWITCH;
                }
                st = ST_LOCKED;
                return GPSB_LOCKED;
            }
            if (now - since >= GPSB_TRY_MS) {
                nextScanBaud();
                valid = 0;
                since = now;
                nmea.reset();
                return GPSB_TRY;
            }
            return GPSB_IDLE;

        case ST_VERIFY:
            if (valid >= GPSB_NEED) {
                st = ST_LOCKED;
                valid = 0;
                since = now;
                return GPSB_LOCKED;
            }
            if (now - since >= GPSB_VERIFY_MS) {
                // GPS hat nicht umgestellt: bei der gefundenen Rate bleiben
                failed = true;
                cur = found;
                st = ST_LOCKED;
                valid = 0;
                since = now;
                nmea.reset();
                return GPSB_LOCKED;
            }
            return GPSB_IDLE;

        default:   // ST_LOCKED
            if (valid) { valid = 0; since = now; return GPSB_IDLE; }
            if (now - since >= GPSB_LOST_MS) {
                uint32_t m = maxB;
                begin(now, m);
                started = true;
                return GPSB_LOST;
            }
            return GPSB_IDLE;
    }
}

// ── Befehle ──────────────────────────────────────────────────────────────
uint16_t gpsCmdCasicNmea(const char* body, char* out, uint16_t max) {
    uint8_t cs = 0;
    for (const char* p = body; *p; ++p) cs ^= (uint8_t)*p;
    int n = snprintf(out, max, "$%s*%02X\r\n", body, cs);
    return (n > 0 && n < max) ? (uint16_t)n : 0;
}

uint16_t gpsCmdCasicBaud(uint32_t baud, char* out, uint16_t max) {
    static const uint32_t rates[] = {4800, 9600, 19200, 38400, 57600, 115200};
    for (uint8_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        if (rates[i] != baud) continue;
        char body[16];
        snprintf(body, sizeof(body), "PCAS01,%u", (unsigned)i);
        return gpsCmdCasicNmea(body, out, max);
    }
    return 0;
}

uint16_t gpsCmdUbx(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t len, uint8_t* out, uint16_t max) {
    if ((uint32_t)len + 8 > max) return 0;
    out[0] = 0xB5; out[1] = 0x62; out[2] = cls; out[3] = id;
    out[4] = (uint8_t)(len & 0xFF); out[5] = (uint8_t)(len >> 8);
    if (len) memcpy(out + 6, payload, len);
    uint8_t a = 0, b = 0;
    for (uint16_t i = 2; i < 6 + len; i++) { a += out[i]; b += a; }
    out[6 + len] = a;
    out[7 + len] = b;
    return (uint16_t)(len + 8);
}

static void putU16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void putU32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

uint16_t gpsCmdUbxBaud(uint32_t baud, uint8_t* out, uint16_t max) {
    uint8_t p[20] = {0};
    p[0] = 1;                    // UART1
    putU32(p + 4, 0x000008D0);   // 8N1
    putU32(p + 8, baud);
    putU16(p + 12, 0x0003);      // Eingang UBX + NMEA
    putU16(p + 14, 0x0002);      // Ausgang nur NMEA
    return gpsCmdUbx(0x06, 0x00, p, sizeof(p), out, max);
}

uint16_t gpsCmdUbxBaudM10(uint32_t baud, uint8_t* out, uint16_t max) {
    uint8_t p[12] = {0};
    p[1] = 0x01;                 // Ebene RAM
    putU32(p + 4, 0x40520001);   // CFG-UART1-BAUDRATE
    putU32(p + 8, baud);
    return gpsCmdUbx(0x06, 0x8A, p, sizeof(p), out, max);
}

uint16_t gpsCmdUbxRate(uint16_t ms, uint8_t* out, uint16_t max) {
    uint8_t p[6];
    putU16(p, ms);
    putU16(p + 2, 1);            // jede Messung ausgeben
    putU16(p + 4, 1);            // GPS-Zeit
    return gpsCmdUbx(0x06, 0x08, p, sizeof(p), out, max);
}

uint16_t gpsCmdUbxRateM10(uint16_t ms, uint8_t* out, uint16_t max) {
    uint8_t p[10] = {0};
    p[1] = 0x01;
    putU32(p + 4, 0x30210001);   // CFG-RATE-MEAS
    putU16(p + 8, ms);
    return gpsCmdUbx(0x06, 0x8A, p, sizeof(p), out, max);
}

uint16_t gpsCmdUbxMsgOff(uint8_t nmeaId, uint8_t* out, uint16_t max) {
    uint8_t p[3] = {0xF0, nmeaId, 0};
    return gpsCmdUbx(0x06, 0x01, p, sizeof(p), out, max);
}

uint16_t gpsRateForBaud(uint32_t baud) {
    return baud >= 38400 ? 100 : 200;
}
