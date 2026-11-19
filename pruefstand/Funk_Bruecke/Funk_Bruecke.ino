/*
 * Funk_Bruecke.ino  -  Pruefstand "Module per Funk verbinden" (ESP-NOW)
 * Autor: PiperPilot
 *
 * Eine Funk-Bruecke ist ein kleiner ESP32 (S3-Zero, C3 oder klassischer
 * ESP32), der CRSF vom Kabel per ESP-NOW weiterfunkt und umgekehrt:
 *
 *   ELRS-Empfaenger --Kabel--> Bruecke 1  ~~ ESP-NOW ~~>  Bruecke 2 --Kabel--> Modul
 *
 * Die Bruecke ist "durchsichtig": jedes gueltige CRSF-Frame vom Kabel wird
 * gefunkt, jedes gefunkte Frame wird auf das Kabel geschrieben. Dadurch
 * laufen Kanaele, Telemetrie und das Lua-Menue des Senders ohne Aenderung
 * an der Firmware der Module. Beide Bruecken haben dieselbe Firmware, es
 * gibt keine Rolle einzustellen. Mehrere Bruecken mit gleicher
 * Fahrzeug-Kennung bilden ein Netz (Rundruf an alle).
 *
 * Failsafe: Kommen keine Funk-Frames mehr an, schreibt die Bruecke nichts
 * mehr auf das Kabel. Das Modul sieht dann "kein CRSF" und geht in seinen
 * eigenen Failsafe, genau wie bei abgezogenem Kabel.
 *
 * Messwerte (1x pro Sekunde): USB-Konsole (115200) und WebUI.
 *   - Pakete pro Sekunde, verlorene Pakete (Luecken in der Paketnummer)
 *   - Aussetzer > 200 ms und laengste Luecke
 *   - Funk-Laufzeit (Ping hin und zurueck, halbe Zeit = eine Richtung)
 *   - Kanal-Frames pro Sekunde (vom Kabel bzw. aus dem Funk)
 *
 * Bedienung:
 *   - WLAN "Funk-Bruecke-XXXX" (Passwort 123456789), http://192.168.1.1
 *   - BOOT-Taste 2 s halten = WLAN an/aus (WLAN laeuft auf demselben
 *     Kanal wie ESP-NOW)
 *   - LED: gruen blinkt langsam = Funk-Verbindung da, gruen schnell = Funk
 *     da, aber keine Kanaele, rot = keine Funk-Verbindung, blau kurz =
 *     WLAN an (alle 2 s)
 *
 * Arduino IDE: esp32-Core 3.x. S3: "ESP32S3 Dev Module", USB CDC On Boot an.
 * C3: "ESP32C3 Dev Module", USB CDC On Boot an. Klassisch: "ESP32 Dev Module".
 *
 * Versionen:
 *   0.1.0  Erster Pruefstand: durchsichtige Bruecke, Messwerte, WebUI
 *   0.1.1  C3: BOOT-Taste ist GPIO9 (nicht GPIO0)
 *   0.1.2  Neustart der Gegenstelle erkennen: Paketnummer neu synchronisieren
 *          (vorher verwarf die andere Bruecke danach alle Daten als "alt")
 *   0.1.3  WLAN-Blitz blau laenger (0,3 s) und heller
 *   0.1.4  LED-Reihenfolge Standard GRB auch beim S3-Zero (an Martins Boards
 *          geprueft); neuer NVS-Schluessel, damit alte Einstellung nicht greift
 *   0.1.5  UART pro loop()-Durchlauf begrenzen (Bruecke am Empfaenger hing
 *          sonst im Lesen fest: LED dauerhaft an, kein Ping, keine WebUI)
 *   0.1.6  Pull-up am CRSF-RX, damit ein offener Pin keine Stoerdaten liefert
 *   0.1.7  eigene MAC richtig lesen (war 00:00:...): WLAN-Name je Bruecke
 *          verschieden, Laufzeitmessung (Ping) funktioniert
 *   0.1.8  Verzoegerung beseitigt: USB-Ausgabe blockiert nicht mehr, wenn
 *          kein serieller Monitor offen ist; veraltete Kanal-Frames werden
 *          bei Rueckstau verworfen; UART-Sendepuffer; Zaehler fuer
 *          Warteschlange voll / verworfen in der WebUI
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "bruecke_types.h"

#define FW_VERSION "0.1.8"

// ── Pins je Chip ──────────────────────────────────────────────────────
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3-Zero: wie Port B von Router/Sound Mini, LED = WS2812 an GPIO21
  #define BOARD_NAME   "ESP32-S3"
  #define PIN_CRSF_RX  5
  #define PIN_CRSF_TX  6
  #define PIN_LED      21
  #define LED_WS2812   1
  #define LED_RGB_DEFAULT false   // Martins S3-Zero (Bruecken) sind GRB; manche Zero sind RGB -> WebUI
  #define PIN_BOOT     0
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3 Pro Mini / SuperMini: wie Port A des Sound Mini C3.
  // LED an GPIO8 (WS2812) ist noch nicht bestaetigt.
  #define BOARD_NAME   "ESP32-C3"
  #define PIN_CRSF_RX  20
  #define PIN_CRSF_TX  21
  #define PIN_LED      8
  #define LED_WS2812   1
  #define LED_RGB_DEFAULT false
  #define PIN_BOOT     9          // BOOT-Taste beim C3 = GPIO9
#else
  // Klassischer ESP32 (DevKit): UART2-Standardpins, blaue LED an GPIO2
  #define BOARD_NAME   "ESP32"
  #define PIN_CRSF_RX  16
  #define PIN_CRSF_TX  17
  #define PIN_LED      2
  #define LED_WS2812   0
  #define LED_RGB_DEFAULT false
  #define PIN_BOOT     0
#endif

#define BOOT_HOLD_MS   2000
#define LINK_TIMEOUT_MS 200      // so lange ohne Funk-Paket = Aussetzer
#define PING_PERIOD_MS 200
#define MAX_PEERS      8

// ── Einstellungen (NVS) ───────────────────────────────────────────────
struct Settings {
    uint8_t  group;        // Fahrzeug-Kennung 1..255
    uint8_t  channel;      // WLAN/ESP-NOW-Kanal 1..13
    uint8_t  txPower;      // Sendeleistung in dBm (2..20)
    uint32_t baud;         // CRSF-Baudrate am Kabel
    uint8_t  rcInterval;   // Kanal-Frames hoechstens alle n ms funken (0 = alle)
    bool     ledRgb;       // LED-Reihenfolge RGB (S3-Zero) statt GRB
    bool     wifiOn;       // WLAN beim Start an
};
static Settings cfg;
static Preferences prefs;

static void loadSettings() {
    prefs.begin("funkbr", true);
    cfg.group      = prefs.getUChar("group", 1);
    cfg.channel    = prefs.getUChar("chan", 1);
    cfg.txPower    = prefs.getUChar("txp", 8);
    cfg.baud       = prefs.getUInt("baud", 420000);
    cfg.rcInterval = prefs.getUChar("rcint", 0);
    cfg.ledRgb     = prefs.getBool("ledrgb2", LED_RGB_DEFAULT);
    cfg.wifiOn     = prefs.getBool("wifi", true);
    prefs.end();
    if (cfg.group == 0) cfg.group = 1;
    if (cfg.channel < 1 || cfg.channel > 13) cfg.channel = 1;
    if (cfg.txPower < 2 || cfg.txPower > 20) cfg.txPower = 8;
}

static void saveSettings() {
    prefs.begin("funkbr", false);
    prefs.putUChar("group", cfg.group);
    prefs.putUChar("chan",  cfg.channel);
    prefs.putUChar("txp",   cfg.txPower);
    prefs.putUInt ("baud",  cfg.baud);
    prefs.putUChar("rcint", cfg.rcInterval);
    prefs.putBool ("ledrgb2", cfg.ledRgb);
    prefs.putBool ("wifi",  cfg.wifiOn);
    prefs.end();
}

// ── Funk-Paket ────────────────────────────────────────────────────────
// Kopf (7 Byte) + Nutzdaten. ESP-NOW erlaubt 250 Byte, ein CRSF-Frame hat
// hoechstens 64 Byte.
enum : uint8_t { PKT_DATA = 1, PKT_PING = 2, PKT_PONG = 3 };
struct __attribute__((packed)) PktHdr {
    uint8_t  magic0, magic1;   // 'P','F'
    uint8_t  version;          // Paketformat 1
    uint8_t  group;            // Fahrzeug-Kennung
    uint8_t  type;             // PKT_*
    uint16_t seq;              // fortlaufend je Sender (nur DATA)
};
struct __attribute__((packed)) PingBody { uint32_t tUs; };
struct __attribute__((packed)) PongBody { uint8_t to[6]; uint32_t tUs; };

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static QueueHandle_t rxQueue;

// ── Statistik ─────────────────────────────────────────────────────────
static Peer peers[MAX_PEERS];
static uint8_t myMac[6];

struct Counters {
    uint32_t uartIn, uartOut, crcErr, rcUart, rcRadio, sendErr, txPkts, queueFull, rcStale;
};
static Counters cnt, cntSec, cntLast;   // gesamt, Stand der letzten Sekunde, Rate
static uint32_t lastRcRadioMs = 0, lastRcUartMs = 0, lastRcSentMs = 0;
static uint16_t txSeq = 0;

static Peer* findPeer(const uint8_t* mac, bool create) {
    for (auto& p : peers) if (p.used && memcmp(p.mac, mac, 6) == 0) return &p;
    if (!create) return nullptr;
    for (auto& p : peers) if (!p.used) {
        memset(&p, 0, sizeof(p));
        p.used = true; memcpy(p.mac, mac, 6);
        return &p;
    }
    return nullptr;
}

static String macStr(const uint8_t* m) {
    char b[18];
    snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(b);
}

// ── CRSF ──────────────────────────────────────────────────────────────
static uint8_t crc8_d5(const uint8_t* p, uint8_t n) {
    uint8_t c = 0;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0xD5) : (uint8_t)(c << 1);
    }
    return c;
}

// Frame = [Adresse][Laenge][Typ][Nutzdaten...][CRC], Laenge = Typ..CRC (2..62)
static uint8_t uBuf[64];
static uint8_t uPos = 0;
static uint32_t uLastByteMs = 0;

static bool isCrsfAddr(uint8_t a) {
    // gueltige Sync-/Zieladressen: 0xC8 (Flugsteuerung/Empfaenger-Ausgang),
    // 0xEA (Sender), 0xEE (TX-Modul), 0xEC (Empfaenger), 0xEF, 0xCF,
    // allgemein 0x00..0xFF moeglich - wir akzeptieren alles >= 0x80 sowie
    // 0x00 (Broadcast) und pruefen per CRC.
    return a == 0x00 || a >= 0x80;
}

static void sendRaw(const uint8_t* data, size_t len) {
    esp_err_t e = esp_now_send(BCAST, data, len);
    if (e == ESP_OK) cnt.txPkts++; else cnt.sendErr++;
}

static void radioSendFrame(const uint8_t* f, uint8_t flen) {
    uint8_t pkt[sizeof(PktHdr) + 64];
    PktHdr* h = (PktHdr*)pkt;
    h->magic0 = 'P'; h->magic1 = 'F'; h->version = 1;
    h->group = cfg.group; h->type = PKT_DATA; h->seq = txSeq++;
    memcpy(pkt + sizeof(PktHdr), f, flen);
    sendRaw(pkt, sizeof(PktHdr) + flen);
}

static void onUartFrame(const uint8_t* f, uint8_t flen) {
    cnt.uartIn++;
    if (f[2] == 0x16) {                        // RC Channels Packed
        cnt.rcUart++;
        uint32_t now = millis();
        lastRcUartMs = now;
        if (cfg.rcInterval && now - lastRcSentMs < cfg.rcInterval) return;
        lastRcSentMs = now;
    }
    radioSendFrame(f, flen);
}

static void pollUart() {
    // hoechstens 128 Byte pro Durchlauf, damit loop() (LED, Ping, WebUI)
    // auch bei dauerndem Datenstrom vom Empfaenger weiterlaeuft
    int budget = 128;
    while (budget-- > 0 && Serial1.available()) {
        uint8_t b = Serial1.read();
        uint32_t now = millis();
        if (uPos && now - uLastByteMs > 5) uPos = 0;   // Pause = neues Frame
        uLastByteMs = now;
        if (uPos == 0) {
            if (!isCrsfAddr(b)) continue;
            uBuf[uPos++] = b;
            continue;
        }
        if (uPos == 1) {
            if (b < 2 || b > 62) { uPos = 0; if (isCrsfAddr(b)) uBuf[uPos++] = b; continue; }
            uBuf[uPos++] = b;
            continue;
        }
        uBuf[uPos++] = b;
        uint8_t total = uBuf[1] + 2;
        if (uPos >= total) {
            if (crc8_d5(uBuf + 2, uBuf[1] - 1) == uBuf[total - 1]) onUartFrame(uBuf, total);
            else cnt.crcErr++;
            uPos = 0;
        }
    }
}

// ── ESP-NOW Empfang (WLAN-Task) ───────────────────────────────────────
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < (int)sizeof(PktHdr) || len > 250) return;
    const PktHdr* h = (const PktHdr*)data;
    if (h->magic0 != 'P' || h->magic1 != 'F' || h->version != 1 || h->group != cfg.group) return;
    RxItem it;
    memcpy(it.mac, info->src_addr, 6);
    it.len = (uint8_t)len;
    memcpy(it.data, data, len);
    if (info->rx_ctrl) {
        // Empfangsstaerke direkt im Peer ablegen (nur ein int8, unkritisch)
        Peer* p = findPeer(info->src_addr, false);
        if (p) p->rssi = info->rx_ctrl->rssi;
    }
    if (xQueueSend(rxQueue, &it, 0) != pdTRUE) cnt.queueFull++;
}

static void handleRadio(const RxItem& it) {
    const PktHdr* h = (const PktHdr*)it.data;
    const uint8_t* body = it.data + sizeof(PktHdr);
    int blen = it.len - sizeof(PktHdr);
    uint32_t now = millis();
    Peer* p = findPeer(it.mac, true);
    if (!p) return;
    p->lastAnyMs = now;

    switch (h->type) {
    case PKT_DATA: {
        if (p->haveSeq) {
            uint16_t d = (uint16_t)(h->seq - p->lastSeq);
            if (d == 0) break;                             // doppelt
            // Nummer springt zurueck = Gegenstelle wurde neu gestartet:
            // neu synchronisieren statt alles als "alt" zu verwerfen
            // (v0.1.1 hat dann bis zu Minuten lang nichts weitergegeben).
            if (d > 30000) d = 1;
            if (d > 1) p->lost += d - 1;
            uint32_t gap = now - p->lastMs;
            if (gap > LINK_TIMEOUT_MS) p->dropouts++;
            if (gap > p->maxGapMs)  p->maxGapMs  = gap;
            if (gap > p->gapSecMax) p->gapSecMax = gap;
        }
        p->haveSeq = true; p->lastSeq = h->seq; p->lastMs = now;
        p->rx++; p->rxSec++;
        if (blen < 4 || blen > 64 || body[1] + 2 != blen) break;
        if (crc8_d5(body + 2, body[1] - 1) != body[blen - 1]) { cnt.crcErr++; break; }
        // Rueckstau: liegen schon neuere Pakete in der Warteschlange, ist
        // dieses Kanal-Frame veraltet -> nicht mehr aufs Kabel (sonst
        // laeuft die Verzoegerung auf). Andere Frames (Lua, Telemetrie)
        // werden immer weitergegeben.
        if (body[2] == 0x16 && uxQueueMessagesWaiting(rxQueue) > 2) { cnt.rcStale++; lastRcRadioMs = now; break; }
        Serial1.write(body, blen);
        cnt.uartOut++;
        if (body[2] == 0x16) { cnt.rcRadio++; lastRcRadioMs = now; }
        break;
    }
    case PKT_PING: {
        if (blen < (int)sizeof(PingBody)) break;
        uint8_t pkt[sizeof(PktHdr) + sizeof(PongBody)];
        PktHdr* o = (PktHdr*)pkt;
        o->magic0 = 'P'; o->magic1 = 'F'; o->version = 1;
        o->group = cfg.group; o->type = PKT_PONG; o->seq = 0;
        PongBody* pb = (PongBody*)(pkt + sizeof(PktHdr));
        memcpy(pb->to, it.mac, 6);
        pb->tUs = ((const PingBody*)body)->tUs;
        sendRaw(pkt, sizeof(pkt));
        break;
    }
    case PKT_PONG: {
        if (blen < (int)sizeof(PongBody)) break;
        const PongBody* pb = (const PongBody*)body;
        if (memcmp(pb->to, myMac, 6) != 0) break;
        uint32_t rtt = (uint32_t)micros() - pb->tUs;
        p->rttUs = rtt;
        if (rtt > p->rttMaxUs) p->rttMaxUs = rtt;
        p->rttSumUs += rtt; p->rttCnt++;
        break;
    }
    }
}

static void sendPing() {
    uint8_t pkt[sizeof(PktHdr) + sizeof(PingBody)];
    PktHdr* h = (PktHdr*)pkt;
    h->magic0 = 'P'; h->magic1 = 'F'; h->version = 1;
    h->group = cfg.group; h->type = PKT_PING; h->seq = 0;
    ((PingBody*)(pkt + sizeof(PktHdr)))->tUs = micros();
    sendRaw(pkt, sizeof(pkt));
}

// ── Funk starten ──────────────────────────────────────────────────────
static bool wifiApOn = false;
static char apName[32];

static void radioBegin() {
    WiFi.mode(WIFI_STA);               // STA-Schnittstelle fuer ESP-NOW (ohne Anmelden)
    WiFi.setSleep(false);              // kein Stromsparen = kurze Laufzeit
    esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_max_tx_power(cfg.txPower * 4);   // Einheit 0,25 dBm
    // WiFi.macAddress() liefert hier beim Core 3.x noch 00:00:..., daher
    // direkt aus dem WLAN-Treiber (bzw. eFuse) lesen
    if (esp_wifi_get_mac(WIFI_IF_STA, myMac) != ESP_OK || !(myMac[0] | myMac[1] | myMac[2] | myMac[3] | myMac[4] | myMac[5]))
        esp_read_mac(myMac, ESP_MAC_WIFI_STA);
    if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW Start fehlgeschlagen"); return; }
    esp_now_register_recv_cb(onRecv);
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, BCAST, 6);
    pi.channel = cfg.channel;
    pi.ifidx   = WIFI_IF_STA;
    pi.encrypt = false;
    esp_now_add_peer(&pi);
}

// ── WebUI ─────────────────────────────────────────────────────────────
static WebServer web(80);

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Funk-Bruecke</title><style>
body{font-family:system-ui,sans-serif;margin:0;padding:16px;background:#f4f5f7;color:#222}
h1{font-size:20px;margin:0 0 4px}small{color:#666}
.card{background:#fff;border-radius:10px;padding:12px 14px;margin:12px 0;box-shadow:0 1px 3px #0002}
table{border-collapse:collapse;width:100%}td,th{padding:4px 6px;text-align:left;border-bottom:1px solid #eee;font-size:14px}
th{color:#555;font-weight:600}.ok{color:#18833a;font-weight:600}.bad{color:#c62828;font-weight:600}
label{display:block;margin:8px 0 2px;font-size:14px}input,select{font-size:16px;padding:6px;width:100%;box-sizing:border-box}
button{margin-top:12px;font-size:16px;padding:8px 14px;border:0;border-radius:6px;background:#1565c0;color:#fff}
</style></head><body>
<h1>Funk-Br&uuml;cke <span id="ver"></span></h1><small id="me"></small>
<div class="card"><b>Status:</b> <span id="st">...</span>
<table id="own"></table></div>
<div class="card"><b>Gegenstellen</b><table id="peers"></table></div>
<div class="card"><b>Einstellungen</b> <small>(beide Br&uuml;cken gleich einstellen; Speichern startet neu)</small>
<form id="f">
<label>Fahrzeug-Kennung (1-255)<input name="group" type="number" min="1" max="255"></label>
<label>Funkkanal (1-13)<input name="chan" type="number" min="1" max="13"></label>
<label>Sendeleistung<select name="txp"><option value="2">2 dBm</option><option value="5">5 dBm</option><option value="8">8 dBm</option><option value="11">11 dBm</option><option value="15">15 dBm</option><option value="20">20 dBm</option></select></label>
<label>CRSF-Baudrate<select name="baud"><option>115200</option><option>400000</option><option>416666</option><option>420000</option><option>921600</option></select></label>
<label>Kan&auml;le h&ouml;chstens alle ... ms funken (0 = jedes Frame)<input name="rcint" type="number" min="0" max="50"></label>
<label>LED-Reihenfolge<select name="ledrgb"><option value="1">RGB (S3-Zero)</option><option value="0">GRB</option></select></label>
<label>WLAN beim Start<select name="wifi"><option value="1">an</option><option value="0">aus (BOOT 2 s = an)</option></select></label>
<button>Speichern</button></form></div>
<script>
const $=id=>document.getElementById(id);
function row(a,b){return '<tr><th>'+a+'</th><td>'+b+'</td></tr>'}
async function load(){try{const s=await (await fetch('/api/stats')).json();
$('ver').textContent='v'+s.ver;$('me').textContent=s.board+' · '+s.mac+' · Kennung '+s.group+' · Kanal '+s.chan+' · '+s.txp+' dBm';
$('st').innerHTML=s.link?'<span class="ok">Funk-Verbindung da</span>':'<span class="bad">keine Funk-Verbindung</span>';
$('own').innerHTML=row('Kanal-Frames vom Kabel /s',s.rcUart)+row('Kanal-Frames aus Funk /s',s.rcRadio)+
row('Frames Kabel → Funk /s',s.uartIn)+row('Frames Funk → Kabel /s',s.uartOut)+row('Gesendete Pakete /s',s.tx)+
row('CRC-Fehler gesamt',s.crcErr)+row('Sendefehler gesamt',s.sendErr)+row('Warteschlange voll gesamt',s.qFull)+row('Veraltete Kanal-Frames verworfen',s.stale)+row('Laufzeit',s.up+' s');
let h='<tr><th>Gegenstelle</th><th>Pakete/s</th><th>verloren</th><th>Aussetzer &gt;200ms</th><th>l&auml;ngste L&uuml;cke</th><th>Laufzeit (eine Richtung)</th><th>RSSI</th></tr>';
for(const p of s.peers){h+='<tr><td>'+p.mac.slice(-5)+(p.alive?'':' <span class="bad">weg</span>')+'</td><td>'+p.rx+'</td><td>'+p.lost+' ('+p.lossPct+' %)</td><td>'+p.drop+
'</td><td>'+p.gapSec+' / '+p.maxGap+' ms</td><td>'+(p.rtt/2000).toFixed(2)+' ms (max '+(p.rttMax/2000).toFixed(2)+')</td><td>'+p.rssi+' dBm</td></tr>'}
$('peers').innerHTML=s.peers.length?h:'<tr><td>noch keine Gegenstelle empfangen</td></tr>';}catch(e){}}
async function loadCfg(){const c=await (await fetch('/api/config')).json();const f=$('f');for(const k in c)if(f[k])f[k].value=c[k];}
$('f').onsubmit=async e=>{e.preventDefault();await fetch('/api/config',{method:'POST',body:new URLSearchParams(new FormData($('f')))});alert('Gespeichert, Neustart ...');};
loadCfg();load();setInterval(load,1000);
</script></body></html>)HTML";

static uint32_t upSec = 0;

static void handleStats() {
    uint32_t now = millis();
    String j = "{";
    j += "\"ver\":\"" FW_VERSION "\",\"board\":\"" BOARD_NAME "\",\"mac\":\"" + macStr(myMac) + "\"";
    j += ",\"group\":" + String(cfg.group) + ",\"chan\":" + String(cfg.channel) + ",\"txp\":" + String(cfg.txPower);
    bool link = false;
    for (auto& p : peers) if (p.used && p.lastAnyMs && now - p.lastAnyMs <= LINK_TIMEOUT_MS) link = true;
    j += ",\"link\":" + String(link ? "true" : "false");
    j += ",\"rcUart\":" + String(cntLast.rcUart) + ",\"rcRadio\":" + String(cntLast.rcRadio);
    j += ",\"uartIn\":" + String(cntLast.uartIn) + ",\"uartOut\":" + String(cntLast.uartOut);
    j += ",\"tx\":" + String(cntLast.txPkts) + ",\"crcErr\":" + String(cnt.crcErr) + ",\"sendErr\":" + String(cnt.sendErr);
    j += ",\"qFull\":" + String(cnt.queueFull) + ",\"stale\":" + String(cnt.rcStale);
    j += ",\"up\":" + String(upSec) + ",\"peers\":[";
    bool first = true;
    for (auto& p : peers) {
        if (!p.used) continue;
        if (!first) j += ",";
        first = false;
        uint32_t tot = p.rx + p.lost;
        float pct = tot ? 100.0f * p.lost / tot : 0;
        j += "{\"mac\":\"" + macStr(p.mac) + "\",\"alive\":" + String(p.lastAnyMs && now - p.lastAnyMs <= LINK_TIMEOUT_MS ? "true" : "false");
        j += ",\"rx\":" + String(p.rxSecLast) + ",\"lost\":" + String(p.lost) + ",\"lossPct\":" + String(pct, 2);
        j += ",\"drop\":" + String(p.dropouts) + ",\"gapSec\":" + String(p.gapSecLast) + ",\"maxGap\":" + String(p.maxGapMs);
        j += ",\"rtt\":" + String(p.rttAvgUs) + ",\"rttMax\":" + String(p.rttMaxUs) + ",\"rssi\":" + String(p.rssi) + "}";
    }
    j += "]}";
    web.send(200, "application/json", j);
}

static void handleConfigGet() {
    String j = "{\"group\":" + String(cfg.group) + ",\"chan\":" + String(cfg.channel) + ",\"txp\":" + String(cfg.txPower) +
               ",\"baud\":" + String(cfg.baud) + ",\"rcint\":" + String(cfg.rcInterval) +
               ",\"ledrgb\":" + String(cfg.ledRgb ? 1 : 0) + ",\"wifi\":" + String(cfg.wifiOn ? 1 : 0) + "}";
    web.send(200, "application/json", j);
}

static void handleConfigPost() {
    if (web.hasArg("group")) cfg.group      = constrain(web.arg("group").toInt(), 1, 255);
    if (web.hasArg("chan"))  cfg.channel    = constrain(web.arg("chan").toInt(), 1, 13);
    if (web.hasArg("txp"))   cfg.txPower    = constrain(web.arg("txp").toInt(), 2, 20);
    if (web.hasArg("baud"))  cfg.baud       = constrain(web.arg("baud").toInt(), 9600, 2000000);
    if (web.hasArg("rcint")) cfg.rcInterval = constrain(web.arg("rcint").toInt(), 0, 50);
    if (web.hasArg("ledrgb")) cfg.ledRgb    = web.arg("ledrgb").toInt() != 0;
    if (web.hasArg("wifi"))  cfg.wifiOn     = web.arg("wifi").toInt() != 0;
    saveSettings();
    web.send(200, "text/plain", "ok");
    delay(300);
    ESP.restart();
}

static void wifiApSet(bool on) {
    if (on == wifiApOn) return;
    if (on) {
        IPAddress ip(192, 168, 1, 1), mask(255, 255, 255, 0);
        WiFi.mode(WIFI_AP_STA);            // AP dazu, STA bleibt fuer ESP-NOW
        WiFi.softAP(apName, "123456789", cfg.channel);
        WiFi.softAPConfig(ip, ip, mask);
        web.begin();
        Serial.printf("WLAN an: \"%s\" (Passwort 123456789), http://192.168.1.1\n", apName);
    } else {
        web.stop();
        WiFi.softAPdisconnect(true);       // zurueck auf reinen STA-Betrieb
        esp_wifi_set_channel(cfg.channel, WIFI_SECOND_CHAN_NONE);
        Serial.println("WLAN aus");
    }
    wifiApOn = on;
}

// ── LED ───────────────────────────────────────────────────────────────
static void ledColor(uint8_t r, uint8_t g, uint8_t b) {
#if LED_WS2812
    if (cfg.ledRgb) neopixelWrite(PIN_LED, g, r, b);
    else            neopixelWrite(PIN_LED, r, g, b);
#else
    digitalWrite(PIN_LED, (r || g || b) ? HIGH : LOW);
#endif
}

static void updateLed() {
    static uint8_t last = 0xFF;
    uint32_t now = millis();
    bool link = false;
    for (auto& p : peers) if (p.used && p.lastAnyMs && now - p.lastAnyMs <= LINK_TIMEOUT_MS) link = true;
    bool rc = (lastRcRadioMs && now - lastRcRadioMs <= LINK_TIMEOUT_MS) ||
              (lastRcUartMs  && now - lastRcUartMs  <= LINK_TIMEOUT_MS);
    uint8_t c;   // 0 aus, 1 gruen, 2 rot, 3 blau
    if (wifiApOn && (now % 2000) < 300)  c = 3;   // 0,3 s blau = WLAN an
    else if (!link)                      c = (now % 1000) < 500 ? 2 : 0;
    else if (rc)                         c = (now % 1000) < 500 ? 1 : 0;
    else                                 c = (now % 200)  < 100 ? 1 : 0;
    if (c == last) return;
    last = c;
    const uint8_t B = 20;
    switch (c) {
        case 1: ledColor(0, B, 0); break;
        case 2: ledColor(B, 0, 0); break;
        case 3: ledColor(0, 0, 60); break;     // blau heller, sonst kaum sichtbar
        default: ledColor(0, 0, 0);
    }
}

// ── Sekundenstatistik ─────────────────────────────────────────────────
static void everySecond() {
    upSec++;
    cntLast.uartIn  = cnt.uartIn  - cntSec.uartIn;
    cntLast.uartOut = cnt.uartOut - cntSec.uartOut;
    cntLast.rcUart  = cnt.rcUart  - cntSec.rcUart;
    cntLast.rcRadio = cnt.rcRadio - cntSec.rcRadio;
    cntLast.txPkts  = cnt.txPkts  - cntSec.txPkts;
    cntSec = cnt;

    uint32_t now = millis();
    Serial.printf("[%lus] Kabel->Funk %lu/s (Kanaele %lu/s) | Funk->Kabel %lu/s (Kanaele %lu/s) | CRC %lu | Sendefehler %lu\n",
                  (unsigned long)upSec, (unsigned long)cntLast.uartIn, (unsigned long)cntLast.rcUart,
                  (unsigned long)cntLast.uartOut, (unsigned long)cntLast.rcRadio,
                  (unsigned long)cnt.crcErr, (unsigned long)cnt.sendErr);
    for (auto& p : peers) {
        if (!p.used) continue;
        p.rttAvgUs = p.rttCnt ? p.rttSumUs / p.rttCnt : 0;
        p.rttSumUs = 0; p.rttCnt = 0;
        p.rxSecLast = p.rxSec; p.rxSec = 0;
        // laufende Luecke mitzaehlen, auch wenn gar nichts mehr kommt
        uint32_t open = (p.lastMs && p.rxSecLast == 0) ? now - p.lastMs : 0;
        p.gapSecLast = (open > 1000 && open > p.gapSecMax) ? open : p.gapSecMax;
        p.gapSecMax = 0;
        uint32_t tot = p.rx + p.lost;
        Serial.printf("   %s: %lu Pakete/s, verloren %lu (%.2f %%), Aussetzer %lu, Luecke %lu/%lu ms, Laufzeit %.2f ms (max %.2f), RSSI %d%s\n",
                      macStr(p.mac).c_str(), (unsigned long)p.rxSecLast, (unsigned long)p.lost,
                      tot ? 100.0f * p.lost / tot : 0.0f, (unsigned long)p.dropouts,
                      (unsigned long)p.gapSecLast, (unsigned long)p.maxGapMs,
                      p.rttAvgUs / 2000.0f, p.rttMaxUs / 2000.0f, p.rssi,
                      (p.lastAnyMs && now - p.lastAnyMs <= LINK_TIMEOUT_MS) ? "" : "  << KEINE VERBINDUNG");
    }
}

// ── Setup / Loop ──────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
    // USB-CDC: ohne offenen seriellen Monitor (z. B. Strom vom PC) wartete
    // jede Ausgabe bis zum Timeout und bremste loop() aus -> Verzoegerung
    Serial.setTxTimeoutMs(0);
#endif
    delay(300);
    loadSettings();
#if !LED_WS2812
    pinMode(PIN_LED, OUTPUT);
#endif
    pinMode(PIN_BOOT, INPUT_PULLUP);
    ledColor(0, 0, 0);

    rxQueue = xQueueCreate(32, sizeof(RxItem));
    Serial1.setRxBufferSize(1024);
    Serial1.setTxBufferSize(1024);     // write() blockiert nicht
    Serial1.begin(cfg.baud, SERIAL_8N1, PIN_CRSF_RX, PIN_CRSF_TX);
    // Pull-up am RX: ohne angeschlossenes Geraet liest ein offener Pin sonst
    // Stoerungen als Daten (UART-Ruhepegel ist HIGH)
    gpio_pullup_en((gpio_num_t)PIN_CRSF_RX);

    radioBegin();
    snprintf(apName, sizeof(apName), "Funk-Bruecke-%02X%02X", myMac[4], myMac[5]);
    web.on("/", [] { web.send_P(200, "text/html", PAGE); });
    web.on("/api/stats", handleStats);
    web.on("/api/config", HTTP_GET, handleConfigGet);
    web.on("/api/config", HTTP_POST, handleConfigPost);
    if (cfg.wifiOn) wifiApSet(true);

    Serial.printf("\nFunk-Bruecke v%s (%s), MAC %s\n", FW_VERSION, BOARD_NAME, macStr(myMac).c_str());
    Serial.printf("Kennung %u, Kanal %u, %u dBm, CRSF %lu Baud an RX%d/TX%d\n",
                  cfg.group, cfg.channel, cfg.txPower, (unsigned long)cfg.baud, PIN_CRSF_RX, PIN_CRSF_TX);
}

void loop() {
    pollUart();

    RxItem it;
    while (xQueueReceive(rxQueue, &it, 0) == pdTRUE) handleRadio(it);

    uint32_t now = millis();
    static uint32_t lastPing = 0, lastSec = 0, bootDown = 0;
    if (now - lastPing >= PING_PERIOD_MS) { lastPing = now; sendPing(); }
    if (now - lastSec >= 1000) { lastSec += 1000; everySecond(); }

    // BOOT 2 s halten = WLAN an/aus
    if (digitalRead(PIN_BOOT) == LOW) {
        if (!bootDown) bootDown = now;
        else if (bootDown != 1 && now - bootDown >= BOOT_HOLD_MS) { wifiApSet(!wifiApOn); bootDown = 1; }
    } else bootDown = 0;

    if (wifiApOn) web.handleClient();
    updateLed();
}
