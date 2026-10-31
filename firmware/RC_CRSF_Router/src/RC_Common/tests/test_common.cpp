/*
 * tests/test_common.cpp  –  RC_Common
 *
 * Unity-Tests fuer die Teile von RC_Common ohne CRSF-Protokoll:
 * UART-Rollen, Lua-Menue (Tabelle), WLAN-Failsafe, Status-LED,
 * Einstellungen, Namen der Flash-Backups. Start mit:  sh RC_Common/tests/run_tests.sh
 */

// Nur fuer die Host-Tests: liegt im Sketch-Ordner, die Arduino-IDE und
// PlatformIO kompilieren die Datei mit, ohne dieses Define bleibt sie leer.
#ifdef RC_COMMON_HOST_TEST

#include <unity.h>
#include <vector>
#include <string.h>
#include "uart_roles.h"
#include "lua_menu.h"
#include "wifi_failsafe.h"
#include "status_led.h"
#include "settings_common.h"
#include "config_backup.h"

class FakeStream : public Stream {
public:
    std::vector<uint8_t> tx;
    int available() override { return 0; }
    int read() override { return -1; }
    size_t write(const uint8_t* b, size_t n) override { tx.insert(tx.end(), b, b + n); return n; }
};

void setUp() {}
void tearDown() {}

// ── UART-Rollen ─────────────────────────────────────────────────────────

void test_role_names_and_options() {
    TEST_ASSERT_EQUAL_STRING("Aus;CRSF;S.Port;GPS;Raw", uartRoleOptions());
    TEST_ASSERT_EQUAL_STRING("GPS", uartRoleName(UROLE_GPS));
    TEST_ASSERT_EQUAL_STRING("Aus", uartRoleName(99));
    TEST_ASSERT_EQUAL_UINT8(UROLE_COUNT, LuaMenu::optionCount(uartRoleOptions()));
}

void test_role_clamp_and_baud() {
    TEST_ASSERT_EQUAL_UINT8(UROLE_OFF, uartRoleClamp(-1));
    TEST_ASSERT_EQUAL_UINT8(UROLE_OFF, uartRoleClamp(UROLE_COUNT));
    TEST_ASSERT_EQUAL_UINT8(UROLE_RAW, uartRoleClamp(UROLE_RAW));
    TEST_ASSERT_EQUAL_UINT32(420000, uartRoleDefaultBaud(UROLE_CRSF));
    TEST_ASSERT_EQUAL_UINT32(57600,  uartRoleDefaultBaud(UROLE_SPORT));
    TEST_ASSERT_EQUAL_UINT32(UART_BAUD_AUTO, uartRoleDefaultBaud(UROLE_GPS));
    TEST_ASSERT_EQUAL_UINT32(0,      uartRoleDefaultBaud(UROLE_OFF));
}

void test_role_duplicate_resolution() {
    bool dup = false;
    TEST_ASSERT_EQUAL_UINT8(UROLE_OFF, uartRoleResolveSecond(UROLE_GPS, UROLE_GPS, &dup));
    TEST_ASSERT_TRUE(dup);
    TEST_ASSERT_EQUAL_UINT8(UROLE_CRSF, uartRoleResolveSecond(UROLE_CRSF, UROLE_CRSF, &dup));
    TEST_ASSERT_FALSE(dup);   // zwei Kettenports sind erlaubt
    TEST_ASSERT_EQUAL_UINT8(UROLE_SPORT, uartRoleResolveSecond(UROLE_GPS, UROLE_SPORT, &dup));
    TEST_ASSERT_FALSE(dup);
}

void test_role_legacy_maps() {
    // Soundmodul Basis: 3 Hott -> Aus, 4 Raw, 5 GPS
    TEST_ASSERT_EQUAL_UINT8(UROLE_OFF, uartRoleFromSoundBasisLegacy(3));
    TEST_ASSERT_EQUAL_UINT8(UROLE_RAW, uartRoleFromSoundBasisLegacy(4));
    TEST_ASSERT_EQUAL_UINT8(UROLE_GPS, uartRoleFromSoundBasisLegacy(5));
    TEST_ASSERT_EQUAL_UINT8(UROLE_SPORT, uartRoleFromSoundBasisLegacy(2));
    // Soundmodul Mini: 3 Raw, 4 GPS
    TEST_ASSERT_EQUAL_UINT8(UROLE_RAW, uartRoleFromSoundMiniLegacy(3));
    TEST_ASSERT_EQUAL_UINT8(UROLE_GPS, uartRoleFromSoundMiniLegacy(4));
    // Multiswitch: unveraendert
    TEST_ASSERT_EQUAL_UINT8(UROLE_GPS, uartRoleFromMultiswitchLegacy(3));
    TEST_ASSERT_EQUAL_UINT8(UROLE_OFF, uartRoleFromMultiswitchLegacy(7));
}

void test_baud_index() {
    TEST_ASSERT_EQUAL_UINT8(UART_BAUD_COUNT, LuaMenu::optionCount(uartBaudOptions()));
    TEST_ASSERT_EQUAL_UINT8(0, uartBaudToIndex(UART_BAUD_AUTO));
    TEST_ASSERT_EQUAL_UINT32(UART_BAUD_AUTO, uartBaudFromIndex(0));
    TEST_ASSERT_EQUAL_UINT8(1, uartBaudToIndex(9600));
    TEST_ASSERT_EQUAL_UINT8(1, uartBaudToIndex(4800));     // "Auto" nur exakt
    TEST_ASSERT_EQUAL_UINT8(7, uartBaudToIndex(420000));
    TEST_ASSERT_EQUAL_UINT8(5, uartBaudToIndex(100000));   // naechstgelegen: 115200
    TEST_ASSERT_EQUAL_UINT32(57600, uartBaudFromIndex(4));
    TEST_ASSERT_EQUAL_UINT32(UART_BAUD_LIST[UART_BAUD_COUNT - 1], uartBaudFromIndex(200));
}

void test_baud_auto_start() {
    TEST_ASSERT_TRUE(uartBaudValid(UART_BAUD_AUTO));
    TEST_ASSERT_FALSE(uartBaudValid(0));
    TEST_ASSERT_FALSE(uartBaudValid(100));
    TEST_ASSERT_EQUAL_UINT32(9600,   uartBaudStart(UROLE_GPS, UART_BAUD_AUTO));
    TEST_ASSERT_EQUAL_UINT32(420000, uartBaudStart(UROLE_CRSF, UART_BAUD_AUTO));
    TEST_ASSERT_EQUAL_UINT32(57600,  uartBaudStart(UROLE_SPORT, UART_BAUD_AUTO));
    TEST_ASSERT_EQUAL_UINT32(115200, uartBaudStart(UROLE_RAW, UART_BAUD_AUTO));
    TEST_ASSERT_EQUAL_UINT32(38400,  uartBaudStart(UROLE_GPS, 38400));
}

// ── Lua-Menue ───────────────────────────────────────────────────────────

static uint8_t g_vals[4];
static uint8_t getVal(uint8_t a) { return g_vals[a]; }
static void    setVal(uint8_t a, uint8_t v) { g_vals[a] = v; }
static void    viewNarrow(uint8_t, LuaView& v, char* buf) {
    v.max = 3;
    strcpy(buf, "Kanal X"); v.name = buf;
}
static void    viewDisabled(uint8_t, LuaView& v, char*) {
    v.type = LUA_INFO; v.text = "Deaktiviert";
}

static LuaItem g_items[16];

static void buildMenu(LuaMenu& m) {
    m.clear();
    m.folder(1, 0, "Ausgaenge");
    m.u8(2, 1, "Helligkeit", 0, 100, "%", getVal, setVal, 0);
    m.select(3, 0, "Port B", uartRoleOptions(), getVal, setVal, 1);
    m.u8(4, 1, "Schmal", 0, 10, "", getVal, setVal, 2)->view = viewNarrow;
    m.info(5, 0, "Version", "1.0");
    m.u8(6, 1, "Aus", 0, 10, "", getVal, setVal, 3)->view = viewDisabled;
}

void test_lua_select_max_from_options() {
    LuaMenu m(g_items, 16); buildMenu(m);
    const LuaItem* it = m.find(3);
    TEST_ASSERT_NOT_NULL(it);
    TEST_ASSERT_EQUAL_UINT8(UROLE_COUNT - 1, it->max);
    TEST_ASSERT_EQUAL_UINT8(6, m.maxId());
    TEST_ASSERT_EQUAL_UINT8(6, m.size());
}

void test_lua_capacity() {
    LuaItem small[2];
    LuaMenu m(small, 2);
    TEST_ASSERT_NOT_NULL(m.folder(1, 0, "a"));
    TEST_ASSERT_NOT_NULL(m.folder(2, 0, "b"));
    TEST_ASSERT_NULL(m.folder(3, 0, "c"));
}

void test_lua_write_clamps_and_rejects() {
    LuaMenu m(g_items, 16); buildMenu(m);
    memset(g_vals, 0, sizeof(g_vals));
    TEST_ASSERT_TRUE(m.write(2, 150));
    TEST_ASSERT_EQUAL_UINT8(100, g_vals[0]);
    TEST_ASSERT_TRUE(m.write(3, 9));
    TEST_ASSERT_EQUAL_UINT8(UROLE_COUNT - 1, g_vals[1]);
    TEST_ASSERT_TRUE(m.write(4, 8));            // Ansicht begrenzt auf 3
    TEST_ASSERT_EQUAL_UINT8(3, g_vals[2]);
    TEST_ASSERT_FALSE(m.write(1, 1));           // Ordner
    TEST_ASSERT_FALSE(m.write(5, 1));           // Info
    TEST_ASSERT_FALSE(m.write(6, 1));           // per view deaktiviert
    TEST_ASSERT_FALSE(m.write(42, 1));          // unbekannt
    TEST_ASSERT_EQUAL_UINT8(0, g_vals[3]);
}

void test_lua_view_override() {
    LuaMenu m(g_items, 16); buildMenu(m);
    char buf[LUA_VIEW_BUF] = {0};
    LuaView v = m.viewOf(*m.find(4), buf);
    TEST_ASSERT_EQUAL_STRING("Kanal X", v.name);
    TEST_ASSERT_EQUAL_UINT8(3, v.max);
}

// Antwort-Frame: EA len 2B dest orig id chunks parent type name...
static bool frameHasChildren(const std::vector<uint8_t>& tx, uint8_t id,
                             const char* name, const std::vector<uint8_t>& children) {
    if (tx.size() < 12 || tx[2] != 0x2B || tx[5] != id) return false;
    size_t p = 9;
    size_t nl = strlen(name);
    if (memcmp(&tx[p], name, nl + 1) != 0) return false;
    p += nl + 1;
    for (uint8_t c : children) if (tx[p++] != c) return false;
    return tx[p] == 0xFF;
}

void test_lua_folder_children_in_insert_order() {
    LuaMenu m(g_items, 16); buildMenu(m);
    CRSF crsf;
    FakeStream io;
    crsf.attach_stream(&io);
    TEST_ASSERT_TRUE(m.send(crsf, 1));
    TEST_ASSERT_TRUE(frameHasChildren(io.tx, 1, "Ausgaenge", {2, 4, 6}));
    io.tx.clear();
    TEST_ASSERT_FALSE(m.send(crsf, 77));
    TEST_ASSERT_EQUAL(0, (int)io.tx.size());
}

// ── WLAN-Failsafe ───────────────────────────────────────────────────────

void test_wifi_failsafe_switches_on_after_timeout() {
    WifiFailsafe f;
    TEST_ASSERT_FALSE(f.check(1000, false, true, 10, false, false));   // Start
    TEST_ASSERT_FALSE(f.check(10999, false, true, 10, false, false));
    TEST_ASSERT_TRUE(f.check(11000, false, true, 10, false, false));
}

void test_wifi_failsafe_resets_on_signal_and_respects_flags() {
    WifiFailsafe f;
    f.check(0, false, true, 10, false, false);
    f.check(9000, false, true, 10, true, false);                       // Signal da
    TEST_ASSERT_FALSE(f.check(12000, false, true, 10, false, false));  // neu starten
    TEST_ASSERT_FALSE(f.check(21999, false, true, 10, false, false));
    TEST_ASSERT_TRUE(f.check(22000, false, true, 10, false, false));
    WifiFailsafe g;
    g.check(0, false, false, 10, false, false);
    TEST_ASSERT_FALSE(g.check(60000, false, false, 10, false, false)); // Auto aus
    TEST_ASSERT_FALSE(g.check(60000, true, true, 10, false, false));   // dauerhaft an
    TEST_ASSERT_FALSE(g.check(60000, false, true, 10, false, true));   // AP laeuft schon
}

// ── Status-LED ──────────────────────────────────────────────────────────

void test_led_modes_crsf() {
    StatusLedInput in = {};
    in.crsf = true;
    TEST_ASSERT_EQUAL_UINT8(SLED_OFF, statusLedMode(in, 5000));
    in.busOk = true;
    TEST_ASSERT_EQUAL_UINT8(SLED_FAST, statusLedMode(in, 5000));       // kein RC-Frame
    in.lastRcFrameMs = 4800;
    TEST_ASSERT_EQUAL_UINT8(SLED_SLOW, statusLedMode(in, 5000));
    in.linkStatsMs = 4900; in.uplinkLQ = 0;
    TEST_ASSERT_EQUAL_UINT8(SLED_FAST, statusLedMode(in, 5000));       // LQ 0
    in.configError = true;
    TEST_ASSERT_EQUAL_UINT8(SLED_DOUBLE, statusLedMode(in, 5000));
}

void test_led_modes_sbus() {
    StatusLedInput in = {};
    TEST_ASSERT_EQUAL_UINT8(SLED_OFF, statusLedMode(in, 5000));
    in.lastSbusFrameMs = 4990;
    TEST_ASSERT_EQUAL_UINT8(SLED_SLOW, statusLedMode(in, 5000));
    in.sbusFailsafe = true;
    TEST_ASSERT_EQUAL_UINT8(SLED_FAST, statusLedMode(in, 5000));
    TEST_ASSERT_EQUAL_UINT8(SLED_OFF, statusLedMode(in, 7100));        // Frames weg
}

void test_led_patterns() {
    TEST_ASSERT_TRUE(statusLedOn(SLED_SLOW, 499));
    TEST_ASSERT_FALSE(statusLedOn(SLED_SLOW, 500));
    TEST_ASSERT_TRUE(statusLedOn(SLED_FAST, 1099));
    TEST_ASSERT_FALSE(statusLedOn(SLED_FAST, 1100));
    TEST_ASSERT_TRUE(statusLedOn(SLED_DOUBLE, 50));
    TEST_ASSERT_FALSE(statusLedOn(SLED_DOUBLE, 200));
    TEST_ASSERT_TRUE(statusLedOn(SLED_DOUBLE, 300));
    TEST_ASSERT_FALSE(statusLedOn(SLED_DOUBLE, 600));
    TEST_ASSERT_FALSE(statusLedOn(SLED_OFF, 0));
}

// ── Einstellungen ───────────────────────────────────────────────────────

void test_settings_sanitize() {
    RcCommonSettings s = {};
    memset(s.ssid, 'x', sizeof(s.ssid));
    s.moduleAddr = 300;
    s.wifiTimeout = 2;
    s.role[0] = 9;  s.baud[0] = 0;
    s.role[1] = UROLE_GPS; s.baud[1] = 100;
    rcSettingsSanitize(s);
    TEST_ASSERT_EQUAL(32, (int)strlen(s.ssid));
    TEST_ASSERT_EQUAL(255, s.moduleAddr);
    TEST_ASSERT_EQUAL_UINT16(60, s.wifiTimeout);
    TEST_ASSERT_EQUAL_UINT8(UROLE_OFF, s.role[0]);
    TEST_ASSERT_EQUAL_UINT32(115200, s.baud[0]);
    TEST_ASSERT_EQUAL_UINT32(UART_BAUD_AUTO, s.baud[1]);   // GPS: ungueltig -> Auto
    s.wifiTimeout = 240; s.baud[1] = 38400;
    rcSettingsSanitize(s);
    TEST_ASSERT_EQUAL_UINT16(240, s.wifiTimeout);
    TEST_ASSERT_EQUAL_UINT32(38400, s.baud[1]);
    s.baud[1] = UART_BAUD_AUTO;
    rcSettingsSanitize(s);
    TEST_ASSERT_EQUAL_UINT32(UART_BAUD_AUTO, s.baud[1]);
}


// ── Einstellungs-Backups im Flash (seit 1.2.0) ──────────────────────────

void test_backup_names() {
    char n[CFG_BACKUP_NAME_LEN];
    cfgBackupMakeName(n, sizeof(n), 12, "20261021-2105", false);
    TEST_ASSERT_EQUAL_STRING("cfg-0012-20261021-2105.json", n);
    bool a = true;
    TEST_ASSERT_EQUAL_INT(12, cfgBackupParseName(n, &a));
    TEST_ASSERT_FALSE(a);
    cfgBackupMakeName(n, sizeof(n), 7, "kaputt", true);
    TEST_ASSERT_EQUAL_STRING("cfg-0007-00000000-0000-auto.json", n);
    TEST_ASSERT_EQUAL_INT(7, cfgBackupParseName(n, &a));
    TEST_ASSERT_TRUE(a);
    TEST_ASSERT_TRUE(strlen(n) < CFG_BACKUP_NAME_LEN);
}

void test_backup_rejects_foreign_names() {
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName(nullptr));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName(""));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("tmp"));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("../cfg-0001-20261021-2105.json"));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("cfg-0001-20261021-2105.json/"));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("cfg-00a1-20261021-2105.json"));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("cfg-0001-2026102x-2105.json"));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("cfg-0001-20261021-2105-otto.json"));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupParseName("cfg-0001-20261021-2105.jsox"));
    TEST_ASSERT_FALSE(cfgBackupWhenValid("2026102-21055"));
    TEST_ASSERT_TRUE(cfgBackupWhenValid("20261021-2105"));
}

void test_backup_prune_per_kind() {
    const char* names[] = {
        "cfg-0003-20261021-2105.json", "cfg-0001-20261021-2100.json",
        "cfg-0002-20261021-2101-auto.json", "fremd.txt",
        "cfg-0004-20261021-2106.json",
    };
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupPruneIndex(names, 5, false, 3));
    TEST_ASSERT_EQUAL_INT(1, cfgBackupPruneIndex(names, 5, false, 2));
    TEST_ASSERT_EQUAL_INT(-1, cfgBackupPruneIndex(names, 5, true, 1));
    TEST_ASSERT_EQUAL_INT(2, cfgBackupPruneIndex(names, 5, true, 0));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_role_names_and_options);
    RUN_TEST(test_role_clamp_and_baud);
    RUN_TEST(test_role_duplicate_resolution);
    RUN_TEST(test_role_legacy_maps);
    RUN_TEST(test_baud_index);
    RUN_TEST(test_baud_auto_start);
    RUN_TEST(test_lua_select_max_from_options);
    RUN_TEST(test_lua_capacity);
    RUN_TEST(test_lua_write_clamps_and_rejects);
    RUN_TEST(test_lua_view_override);
    RUN_TEST(test_lua_folder_children_in_insert_order);
    RUN_TEST(test_wifi_failsafe_switches_on_after_timeout);
    RUN_TEST(test_wifi_failsafe_resets_on_signal_and_respects_flags);
    RUN_TEST(test_led_modes_crsf);
    RUN_TEST(test_led_modes_sbus);
    RUN_TEST(test_led_patterns);
    RUN_TEST(test_settings_sanitize);
    RUN_TEST(test_backup_names);
    RUN_TEST(test_backup_rejects_foreign_names);
    RUN_TEST(test_backup_prune_per_kind);
    return UNITY_END();
}

#endif // RC_COMMON_HOST_TEST
