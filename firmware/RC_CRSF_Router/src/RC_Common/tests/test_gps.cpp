/*
 * tests/test_gps.cpp  –  RC_Common
 *
 * Unity-Tests fuer die GPS-Baudratensuche (gps_autobaud) mit einem
 * simulierten GPS. Start mit:  sh RC_Common/tests/run_tests.sh
 */

// Nur fuer die Host-Tests, siehe test_common.cpp
#ifdef RC_COMMON_HOST_TEST

#include <unity.h>
#include <string.h>
#include "gps_autobaud.h"

void setUp() {}
void tearDown() {}

static const char* GGA = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";

static void feedStr(GpsAutobaud& a, const char* s) { while (*s) a.feed((uint8_t)*s++); }

// Simuliertes GPS: sendet mit gpsBaud, nimmt Umstellbefehle an, wenn switchable
struct SimGps {
    uint32_t gpsBaud;
    bool     switchable;
    bool     connected = true;
    uint32_t sentSwitchTo = 0;
};

// Laeuft bis "gefunden" oder Zeitende; liefert die Zeit in ms (0 = nie)
static uint32_t run(GpsAutobaud& a, SimGps& g, uint32_t start, uint32_t limit) {
    for (uint32_t t = start; t < start + limit; t += 50) {
        if (g.connected && t % 200 == 0) {
            if (a.baud() == g.gpsBaud) feedStr(a, GGA);
            else for (int i = 0; i < 40; i++) a.feed((uint8_t)(0x80 + (i * 37 + t) % 120));   // Zeichensalat
        }
        uint8_t r = a.poll(t);
        if (r == GPSB_SWITCH) {
            g.sentSwitchTo = a.target();
            if (g.switchable) g.gpsBaud = a.target();
        }
        if (r == GPSB_LOCKED) return t - start + 1;
    }
    return 0;
}

void test_nmea_checksum() {
    NmeaChecker n;
    bool ok = false;
    for (const char* p = GGA; *p; ++p) ok |= n.feed((uint8_t)*p);
    TEST_ASSERT_TRUE(ok);
    // falsche Pruefsumme
    ok = false;
    const char* bad = "$GPGGA,123519,4807.038,N*00\r\n";
    for (const char* p = bad; *p; ++p) ok |= n.feed((uint8_t)*p);
    TEST_ASSERT_FALSE(ok);
}

void test_finds_9600_and_switches_to_115200() {
    GpsAutobaud a; a.begin(0);
    SimGps g{9600, true};
    TEST_ASSERT_NOT_EQUAL(0, run(a, g, 0, 10000));
    TEST_ASSERT_TRUE(a.locked());
    TEST_ASSERT_EQUAL_UINT32(115200, g.sentSwitchTo);
    TEST_ASSERT_EQUAL_UINT32(115200, a.baud());
    TEST_ASSERT_FALSE(a.switchFailed());
}

void test_finds_57600() {
    GpsAutobaud a; a.begin(0);
    SimGps g{57600, true};
    TEST_ASSERT_NOT_EQUAL(0, run(a, g, 0, 15000));
    TEST_ASSERT_EQUAL_UINT32(115200, a.baud());
}

void test_already_115200_no_switch() {
    GpsAutobaud a; a.begin(0);
    SimGps g{115200, true};
    TEST_ASSERT_NOT_EQUAL(0, run(a, g, 0, 10000));
    TEST_ASSERT_EQUAL_UINT32(0, g.sentSwitchTo);
    TEST_ASSERT_EQUAL_UINT32(115200, a.baud());
}

void test_switch_fails_stays_on_found() {
    GpsAutobaud a; a.begin(0);
    SimGps g{9600, false};
    TEST_ASSERT_NOT_EQUAL(0, run(a, g, 0, 10000));
    TEST_ASSERT_TRUE(a.switchFailed());
    TEST_ASSERT_EQUAL_UINT32(9600, a.baud());
}

void test_max_baud_softwareserial() {
    GpsAutobaud a; a.begin(0, 38400);
    TEST_ASSERT_EQUAL_UINT32(38400, a.target());
    SimGps g{9600, true};
    TEST_ASSERT_NOT_EQUAL(0, run(a, g, 0, 10000));
    TEST_ASSERT_EQUAL_UINT32(38400, a.baud());
    // 115200 wird nie versucht
    GpsAutobaud b; b.begin(0, 38400);
    for (uint32_t t = 0; t < 20000; t += 50) {
        b.poll(t);
        TEST_ASSERT_TRUE(b.baud() <= 38400);
    }
}

void test_lost_restarts_scan() {
    GpsAutobaud a; a.begin(0);
    SimGps g{9600, true};
    uint32_t t = run(a, g, 0, 10000);
    TEST_ASSERT_NOT_EQUAL(0, t);
    // weitere Saetze halten die Verbindung
    uint32_t now = 20000;
    for (int i = 0; i < 10; i++, now += 500) { feedStr(a, GGA); TEST_ASSERT_EQUAL_UINT8(GPSB_IDLE, a.poll(now)); }
    // GPS weg (Strom aus) -> nach 3 s neue Suche ab 9600
    uint8_t r = GPSB_IDLE;
    for (uint32_t k = 0; k <= GPSB_LOST_MS + 100 && r != GPSB_LOST; k += 100) r = a.poll(now + k);
    TEST_ASSERT_EQUAL_UINT8(GPSB_LOST, r);
    TEST_ASSERT_FALSE(a.locked());
    TEST_ASSERT_EQUAL_UINT32(9600, a.baud());
    // GPS wieder da, nach Stromausfall wieder mit 9600
    g.gpsBaud = 9600;
    TEST_ASSERT_NOT_EQUAL(0, run(a, g, 40000, 10000));
    TEST_ASSERT_EQUAL_UINT32(115200, a.baud());
}

void test_no_gps_keeps_scanning() {
    GpsAutobaud a; a.begin(0);
    SimGps g{9600, true}; g.connected = false;
    TEST_ASSERT_EQUAL_UINT32(0, run(a, g, 0, 30000));
    TEST_ASSERT_FALSE(a.locked());
}

void test_commands() {
    char c[32];
    uint16_t n = gpsCmdCasicBaud(115200, c, sizeof(c));
    TEST_ASSERT_EQUAL_STRING("$PCAS01,5*19\r\n", c);
    TEST_ASSERT_EQUAL_UINT16(14, n);
    TEST_ASSERT_EQUAL_UINT16(0, gpsCmdCasicBaud(230400, c, sizeof(c)));
    // UBX-CFG-RATE 100 ms: bekannte Byte-Folge
    uint8_t u[32];
    n = gpsCmdUbxRate(100, u, sizeof(u));
    const uint8_t rate[] = {0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x00, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12};
    TEST_ASSERT_EQUAL_UINT16(sizeof(rate), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rate, u, sizeof(rate));
    // UBX-CFG-PRT 115200: Laenge 20 + 8, Baudrate an Stelle 6+8
    n = gpsCmdUbxBaud(115200, u, sizeof(u));
    TEST_ASSERT_EQUAL_UINT16(28, n);
    TEST_ASSERT_EQUAL_UINT8(0x00, u[14]); TEST_ASSERT_EQUAL_UINT8(0xC2, u[15]); TEST_ASSERT_EQUAL_UINT8(0x01, u[16]);
    TEST_ASSERT_EQUAL_UINT16(0, gpsCmdUbxBaud(115200, u, 10));   // Puffer zu klein
    TEST_ASSERT_EQUAL_UINT16(100, gpsRateForBaud(115200));
    TEST_ASSERT_EQUAL_UINT16(200, gpsRateForBaud(9600));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nmea_checksum);
    RUN_TEST(test_finds_9600_and_switches_to_115200);
    RUN_TEST(test_finds_57600);
    RUN_TEST(test_already_115200_no_switch);
    RUN_TEST(test_switch_fails_stays_on_found);
    RUN_TEST(test_max_baud_softwareserial);
    RUN_TEST(test_lost_restarts_scan);
    RUN_TEST(test_no_gps_keeps_scanning);
    RUN_TEST(test_commands);
    return UNITY_END();
}

#endif // RC_COMMON_HOST_TEST
