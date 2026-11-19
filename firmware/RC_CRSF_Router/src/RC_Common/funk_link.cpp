// funk_link.cpp - RC_Common, siehe funk_link.h
#include "funk_link.h"
#include <string.h>

const uint8_t FUNK_PWR_VALUES[FUNK_PWR_COUNT]   = {2, 5, 8, 11, 15, 20};
const uint8_t FUNK_RATE_VALUES[FUNK_RATE_COUNT] = {0, 5, 10, 20, 50};

uint8_t funkPwrIndex(uint8_t dbm) {
    for (uint8_t i = 0; i < FUNK_PWR_COUNT; i++) if (FUNK_PWR_VALUES[i] >= dbm) return i;
    return FUNK_PWR_COUNT - 1;
}
uint8_t funkRateIndex(uint8_t ms) {
    for (uint8_t i = 0; i < FUNK_RATE_COUNT; i++) if (FUNK_RATE_VALUES[i] >= ms) return i;
    return FUNK_RATE_COUNT - 1;
}
void funkConfigClamp(FunkConfig& c) {
    if (c.id == 0) c.id = 1;
    if (c.ch < 1 || c.ch > 13) c.ch = 1;
    c.pwr    = FUNK_PWR_VALUES[funkPwrIndex(c.pwr)];
    c.rateMs = FUNK_RATE_VALUES[funkRateIndex(c.rateMs)];
}
const char* funkRoleName(uint8_t role) {
    switch (role) {
        case FUNK_ZENTRALE:   return "Zentrale";
        case FUNK_TEILNEHMER: return "Teilnehmer";
        case FUNK_BRUECKE:    return "Brücke";
        default:              return "Aus";
    }
}

#if defined(ARDUINO) && defined(ESP32)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

void funkConfigLoad(Preferences& p, FunkConfig& c) {
    FunkConfig d;
    c.id     = p.getUChar("fkid",   d.id);
    c.ch     = p.getUChar("fkch",   d.ch);
    c.pwr    = p.getUChar("fkpwr",  d.pwr);
    c.rateMs = p.getUChar("fkrate", d.rateMs);
    c.back   = p.getBool ("fkback", d.back);
    funkConfigClamp(c);
}
void funkConfigSave(Preferences& p, const FunkConfig& c) {
    p.putUChar("fkid",   c.id);
    p.putUChar("fkch",   c.ch);
    p.putUChar("fkpwr",  c.pwr);
    p.putUChar("fkrate", c.rateMs);
    p.putBool ("fkback", c.back);
}

// ── Paketformat (gleich wie Funk-Bruecke v0.1.9, Version 1) ─────────────
enum : uint8_t { PKT_DATA = 1, PKT_PING = 2, PKT_PONG = 3 };
struct __attribute__((packed)) PktHdr {
    uint8_t  magic0, magic1;   // 'P','F'
    uint8_t  version;          // 1
    uint8_t  group;            // Fahrzeug-Kennung
    uint8_t  type;             // PKT_*
    uint16_t seq;              // fortlaufend je Sender (nur DATA)
};
// Bruecke v0.1.9 sendet tUs + name (20 Byte) und liest nur diese; Rolle
// und CRSF-Adresse haengen hinten dran (RC_Common 1.5.0)
struct __attribute__((packed)) PingBody { uint32_t tUs; char name[16]; uint8_t role; uint8_t crsfAddr; };
struct __attribute__((packed)) PongBody { uint8_t to[6]; uint32_t tUs; };

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static constexpr uint32_t PING_MS      = 200;
static constexpr uint32_t GAP_MS       = 200;    // Aussetzer
static constexpr uint32_t ALIVE_MS     = 500;    // "verbunden"
static constexpr uint32_t REACH_MS     = 1000;   // noch Daten hinschicken
static constexpr int      RXQ_LEN      = 16;
static constexpr int      RXQ_DATA     = 96;     // Kopf 7 + Ping 22 / CRSF 64

struct FunkRxItem { uint8_t mac[6]; int8_t rssi; uint8_t len; uint8_t data[RXQ_DATA]; };

static FunkLink* s_funk = nullptr;

static uint8_t crc8d5(const uint8_t* p, uint8_t n) {
    uint8_t c = 0;
    while (n--) {
        c ^= *p++;
        for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0xD5) : (uint8_t)(c << 1);
    }
    return c;
}

static void onRecvCb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!s_funk || !info) return;
    s_funk->onRecvRaw(info->src_addr, data, len, info->rx_ctrl ? (int8_t)info->rx_ctrl->rssi : 0);
}
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
static void onSentCb(const esp_now_send_info_t* info, esp_now_send_status_t st) {
    const uint8_t* mac = info ? info->des_addr : nullptr;
#else
static void onSentCb(const uint8_t* mac, esp_now_send_status_t st) {
#endif
    if (!s_funk) return;
    s_funk->onSentRaw(st == ESP_NOW_SEND_SUCCESS, mac && memcmp(mac, BCAST, 6) != 0);
}

// WLAN-Task: nur pruefen und in die Warteschlange, Auswertung in loop()
void FunkLink::onRecvRaw(const uint8_t* mac, const uint8_t* data, int len, int8_t rssi) {
    if (len < (int)sizeof(PktHdr) || len > RXQ_DATA) return;
    const PktHdr* h = (const PktHdr*)data;
    if (h->magic0 != 'P' || h->magic1 != 'F' || h->version != 1 || h->group != cfg_.id) return;
    FunkRxItem it;
    memcpy(it.mac, mac, 6);
    it.rssi = rssi;
    it.len  = (uint8_t)len;
    memcpy(it.data, data, len);
    if (xQueueSend((QueueHandle_t)rxQueue_, &it, 0) != pdTRUE) qFull_++;
}

void FunkLink::onSentRaw(bool ok, bool unicast) {
    if (!ok && unicast) noAck_++;
}

void FunkLink::setName(const char* name) {
    strlcpy(name_, name ? name : "", sizeof(name_));
}

bool FunkLink::begin(uint8_t role, const FunkConfig& cfg, const char* name, uint8_t crsfAddr) {
    role_ = FUNK_AUS;
    if (role != FUNK_ZENTRALE && role != FUNK_TEILNEHMER) return false;
    cfg_ = cfg;
    funkConfigClamp(cfg_);
    setName(name);
    crsfAddr_ = crsfAddr;
    if (!rxQueue_) rxQueue_ = xQueueCreate(RXQ_LEN, sizeof(FunkRxItem));
    if (!rxQueue_) return false;

    // STA-Schnittstelle fuer ESP-NOW (ohne Anmelden), ein laufender AP bleibt
    wifi_mode_t m = WiFi.getMode();
    if (!(m & WIFI_MODE_STA)) WiFi.mode((wifi_mode_t)(m | WIFI_MODE_STA));
    WiFi.setSleep(false);              // kein Stromsparen = kurze Laufzeit
    esp_wifi_set_channel(cfg_.ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_max_tx_power(cfg_.pwr * 4);   // Einheit 0,25 dBm
    if (esp_wifi_get_mac(WIFI_IF_STA, myMac_) != ESP_OK ||
        !(myMac_[0] | myMac_[1] | myMac_[2] | myMac_[3] | myMac_[4] | myMac_[5]))
        esp_read_mac(myMac_, ESP_MAC_WIFI_STA);
    if (esp_now_init() != ESP_OK) { Serial.println("Funk: ESP-NOW Start fehlgeschlagen"); return false; }
    s_funk = this;
    esp_now_register_recv_cb(onRecvCb);
    esp_now_register_send_cb(onSentCb);
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, BCAST, 6);
    pi.channel = cfg_.ch;
    pi.ifidx   = WIFI_IF_STA;
    pi.encrypt = false;
    esp_now_add_peer(&pi);
    role_ = role;
    startMs_ = lastSecMs_ = millis();
    Serial.printf("Funk: %s, Kennung %u, Kanal %u, %u dBm, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                  funkRoleName(role_), cfg_.id, cfg_.ch, cfg_.pwr,
                  myMac_[0], myMac_[1], myMac_[2], myMac_[3], myMac_[4], myMac_[5]);
    return true;
}

void FunkLink::wifiChanged() {
    if (!active()) return;
    // AP-Start/-Stop kann Kanal und Sendeleistung zuruecksetzen
    esp_wifi_set_channel(cfg_.ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_max_tx_power(cfg_.pwr * 4);
}

// ── Gegenstellen ─────────────────────────────────────────────────────────
FunkLink::Peer* FunkLink::findPeer(const uint8_t* mac, bool create) {
    for (auto& p : peers_) if (p.used && memcmp(p.mac, mac, 6) == 0) return &p;
    if (!create) return nullptr;
    for (auto& p : peers_) if (!p.used) {
        memset(&p, 0, sizeof(p));
        p.used = true;
        memcpy(p.mac, mac, 6);
        p.role = FUNK_BRUECKE;
        return &p;
    }
    return nullptr;
}

void FunkLink::peerRegister(Peer& p) {
    if (p.espAdded) return;
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, p.mac, 6);
    pi.channel = cfg_.ch;
    pi.ifidx   = WIFI_IF_STA;
    pi.encrypt = false;
    esp_err_t e = esp_now_add_peer(&pi);
    p.espAdded = (e == ESP_OK || e == ESP_ERR_ESPNOW_EXIST);
}

// Zentrale <-> Teilnehmer, Bruecken passen zu beiden
bool FunkLink::isPartner(const Peer& p) const {
    if (!p.used || !p.pinged) return false;
    if (role_ == FUNK_ZENTRALE)   return p.role != FUNK_ZENTRALE;
    if (role_ == FUNK_TEILNEHMER) return p.role != FUNK_TEILNEHMER;
    return false;
}

bool FunkLink::reachable(const Peer& p, uint32_t now) const {
    return isPartner(p) && p.espAdded && p.lastAnyMs && now - p.lastAnyMs <= REACH_MS;
}

bool FunkLink::connected() const {
    uint32_t now = millis();
    for (auto& p : peers_) if (isPartner(p) && now - p.lastAnyMs <= ALIVE_MS) return true;
    return false;
}

bool FunkLink::conflict() const {
    if (role_ != FUNK_ZENTRALE) return false;
    uint32_t now = millis();
    for (auto& p : peers_)
        if (p.used && p.pinged && p.role == FUNK_ZENTRALE && now - p.lastAnyMs <= 2000) return true;
    return false;
}

bool FunkLink::dupAddr() const {
    uint32_t now = millis();
    for (auto& p : peers_)
        if (p.used && p.pinged && p.crsfAddr && p.crsfAddr == crsfAddr_ && now - p.lastAnyMs <= 2000) return true;
    return false;
}

uint8_t FunkLink::partnerCount() const {
    uint32_t now = millis();
    uint8_t n = 0;
    for (auto& p : peers_) if (isPartner(p) && now - p.lastAnyMs <= ALIVE_MS) n++;
    return n;
}

static float peerLoss(const FunkLink::Peer& p) {
    uint32_t tot = p.rx + p.lost;
    return tot ? 100.0f * p.lost / tot : 0.0f;
}

float FunkLink::worstLoss() const {
    float w = 0;
    uint32_t now = millis();
    for (auto& p : peers_) if (isPartner(p) && now - p.lastAnyMs <= ALIVE_MS && peerLoss(p) > w) w = peerLoss(p);
    return w;
}

int8_t FunkLink::worstRssi() const {
    int8_t w = 0;
    bool any = false;
    uint32_t now = millis();
    for (auto& p : peers_) if (isPartner(p) && now - p.lastAnyMs <= ALIVE_MS) {
        if (!any || p.rssi < w) w = p.rssi;
        any = true;
    }
    return w;
}

// ── Senden ───────────────────────────────────────────────────────────────
void FunkLink::sendFrame(const uint8_t* f, uint8_t n) {
    if (!active() || n < 4 || n > 64) return;
    uint32_t now = millis();
    if (role_ == FUNK_TEILNEHMER && !cfg_.back) { backDrop_++; return; }   // nur empfangen
    if (f[2] == 0x16) {                                     // RC-Kanaele
        if (role_ == FUNK_ZENTRALE && cfg_.rateMs && now - lastRcSentMs_ < cfg_.rateMs) { rateDrop_++; return; }
        lastRcSentMs_ = now;
        cnt_.rcOut++;
    }
    uint8_t pkt[sizeof(PktHdr) + 64];
    PktHdr* h = (PktHdr*)pkt;
    h->magic0 = 'P'; h->magic1 = 'F'; h->version = 1;
    h->group = cfg_.id; h->type = PKT_DATA; h->seq = txSeq_++;
    memcpy(pkt + sizeof(PktHdr), f, n);
    cnt_.framesOut++;
    bool any = false;
    for (auto& p : peers_) {
        if (!reachable(p, now)) continue;
        any = true;
        if (esp_now_send(p.mac, pkt, sizeof(PktHdr) + n) == ESP_OK) cnt_.sent++;
        else sendErr_++;
    }
    if (!any) noPeer_++;
}

void FunkLink::sendPing() {
    uint8_t pkt[sizeof(PktHdr) + sizeof(PingBody)];
    PktHdr* h = (PktHdr*)pkt;
    h->magic0 = 'P'; h->magic1 = 'F'; h->version = 1;
    h->group = cfg_.id; h->type = PKT_PING; h->seq = 0;
    PingBody* pb = (PingBody*)(pkt + sizeof(PktHdr));
    pb->tUs = micros();
    memset(pb->name, 0, sizeof(pb->name));
    strncpy(pb->name, name_, sizeof(pb->name));
    pb->role = role_;
    pb->crsfAddr = crsfAddr_;
    esp_now_send(BCAST, pkt, sizeof(pkt));   // Rundruf: so finden sich neue Teile
}

// CRSF-Seite schreibt ganze Frames (send_packets, Router); Bytes werden zu
// Frames zusammengesetzt und je Frame gefunkt
size_t FunkLink::write(uint8_t b) { return write(&b, 1); }

size_t FunkLink::write(const uint8_t* buf, size_t len) {
    wrBytes_ += len;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = buf[i];
        if (txPos_ == 1 && (b < 2 || b > 62)) { txPos_ = 0; wrResync_++; continue; }
        tx_[txPos_++] = b;
        if (txPos_ >= 2 && txPos_ >= tx_[1] + 2) {
            sendFrame(tx_, txPos_);
            txPos_ = 0;
        }
    }
    return len;
}

// ── Empfang ──────────────────────────────────────────────────────────────
void FunkLink::ringPut(const uint8_t* f, uint8_t n) {
    if (FUNK_RING - rCount_ < n) { stale_++; return; }   // CRSF-Seite liest nicht
    for (uint8_t i = 0; i < n; i++) ring_[(rHead_ + rCount_ + i) % FUNK_RING] = f[i];
    rCount_ += n;
}

int FunkLink::available() { return rCount_; }
int FunkLink::peek() { return rCount_ ? ring_[rHead_] : -1; }
int FunkLink::read() {
    if (!rCount_) return -1;
    uint8_t b = ring_[rHead_];
    rHead_ = (rHead_ + 1) % FUNK_RING;
    rCount_--;
    return b;
}

void FunkLink::handle(const uint8_t* mac, const uint8_t* data, int len, int8_t rssi) {
    const PktHdr* h = (const PktHdr*)data;
    const uint8_t* body = data + sizeof(PktHdr);
    int blen = len - (int)sizeof(PktHdr);
    uint32_t now = millis();
    Peer* p = findPeer(mac, true);
    if (!p) return;
    if (!p->lastAnyMs || now - p->lastAnyMs > 2000) p->sinceMs = now;
    p->lastAnyMs = now;
    if (rssi) p->rssi = rssi;
    peerRegister(*p);

    switch (h->type) {
    case PKT_DATA: {
        if (!isPartner(*p)) break;   // z. B. Teilnehmer untereinander
        if (p->haveSeq) {
            uint16_t d = (uint16_t)(h->seq - p->lastSeq);
            if (d == 0) break;                   // doppelt
            if (d > 1000) d = 1;                 // Gegenstelle neu gestartet
            if (d > 1) p->lost += d - 1;
            uint32_t gap = now - p->lastMs;
            if (gap > GAP_MS) p->dropouts++;
            if (gap > p->maxGapMs) p->maxGapMs = gap;
        }
        p->haveSeq = true; p->lastSeq = h->seq; p->lastMs = now;
        p->rx++; p->rxSec++;
        lastPartnerMs_ = now;
        if (blen < 4 || blen > 64 || body[1] + 2 != blen) break;
        if (crc8d5(body + 2, body[1] - 1) != body[blen - 1]) { crcErr_++; break; }
        // Rueckstau: liegen neuere Pakete in der Warteschlange, ist dieses
        // Kanal-Frame veraltet (sonst laeuft eine Verzoegerung auf)
        if (body[2] == 0x16 && uxQueueMessagesWaiting((QueueHandle_t)rxQueue_) > 2) { stale_++; break; }
        ringPut(body, (uint8_t)blen);
        cnt_.framesIn++;
        if (body[2] == 0x16) cnt_.rcIn++;
        break;
    }
    case PKT_PING: {
        if (blen < 4) break;
        if (blen >= 20) {
            memcpy(p->name, body + 4, 16);
            p->name[16] = 0;
        }
        if (blen >= (int)sizeof(PingBody)) {
            const PingBody* pb = (const PingBody*)body;
            p->role = (pb->role >= FUNK_ZENTRALE && pb->role <= FUNK_BRUECKE) ? pb->role : FUNK_BRUECKE;
            p->crsfAddr = pb->crsfAddr;
        } else {
            p->role = FUNK_BRUECKE;   // Funk-Bruecke v0.1.x
            p->crsfAddr = 0;
        }
        p->pinged = true;
        // Antwort fuer die Laufzeitmessung der Bruecke
        uint8_t pkt[sizeof(PktHdr) + sizeof(PongBody)];
        PktHdr* o = (PktHdr*)pkt;
        o->magic0 = 'P'; o->magic1 = 'F'; o->version = 1;
        o->group = cfg_.id; o->type = PKT_PONG; o->seq = 0;
        PongBody* pb = (PongBody*)(pkt + sizeof(PktHdr));
        memcpy(pb->to, mac, 6);
        memcpy(&pb->tUs, body, 4);
        if (p->espAdded) esp_now_send(mac, pkt, sizeof(pkt));
        break;
    }
    default:
        break;   // PKT_PONG: keine eigene Laufzeitmessung
    }
}

void FunkLink::everySecond() {
    cntLast_.framesOut = cnt_.framesOut - cntSec_.framesOut;
    cntLast_.framesIn  = cnt_.framesIn  - cntSec_.framesIn;
    cntLast_.rcOut     = cnt_.rcOut     - cntSec_.rcOut;
    cntLast_.rcIn      = cnt_.rcIn      - cntSec_.rcIn;
    cntLast_.sent      = cnt_.sent      - cntSec_.sent;
    cntSec_ = cnt_;
    uint32_t now = millis();
    for (auto& p : peers_) {
        if (!p.used) continue;
        p.rxSecLast = p.rxSec; p.rxSec = 0;
        // lange still: Platz freigeben (z. B. ausgeschaltete Bruecke)
        if (now - p.lastAnyMs > 60000) {
            if (p.espAdded) esp_now_del_peer(p.mac);
            p.used = false;
        }
    }
}

void FunkLink::loop() {
    if (!active()) return;
    FunkRxItem it;
    int budget = 8;   // loop() bleibt bedienbar, Rest beim naechsten Mal
    while (budget-- > 0 && xQueueReceive((QueueHandle_t)rxQueue_, &it, 0) == pdTRUE)
        handle(it.mac, it.data, it.len, it.rssi);
    uint32_t now = millis();
    if (now - lastPingMs_ >= PING_MS) { lastPingMs_ = now; sendPing(); }
    if (now - lastSecMs_ >= 1000) { lastSecMs_ += 1000; everySecond(); }
}

void FunkLink::resetStats() {
    for (auto& p : peers_) {
        if (!p.used) continue;
        p.rx = p.lost = p.dropouts = p.maxGapMs = 0;
    }
    noAck_ = qFull_ = 0;
    sendErr_ = crcErr_ = stale_ = rateDrop_ = 0;
    wrBytes_ = wrResync_ = backDrop_ = noPeer_ = 0;
    cnt_.sent = cntSec_.sent = 0;
}

// ── JSON ─────────────────────────────────────────────────────────────────
static String macText(const uint8_t* m) {
    char b[18];
    snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    return String(b);
}
static String hexAddr(uint8_t a) {
    if (!a) return "-";
    char b[6];
    snprintf(b, sizeof(b), "0x%02X", a);
    return String(b);
}
static String jsonText(const char* s) {
    String o;
    for (; *s; s++) {
        char c = *s;
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((uint8_t)c >= 0x20) o += c;
    }
    return o;
}
static String upText(uint32_t ms) {
    uint32_t s = ms / 1000;
    if (s < 60)   return String(s) + " s";
    if (s < 3600) return String(s / 60) + " min";
    return String(s / 3600) + " h " + String((s / 60) % 60) + " min";
}

String FunkLink::summaryJson() const {
    if (!active()) return "null";
    String j = "{\"role\":\"" + String(funkRoleName(role_)) + "\"";
    j += ",\"connected\":" + String(connected() ? "true" : "false");
    j += ",\"peers\":" + String(partnerCount());
    j += ",\"loss\":" + String(worstLoss(), 1);
    j += ",\"rssi\":" + String(worstRssi());
    j += ",\"conflict\":" + String(conflict() ? "true" : "false");
    j += ",\"dup_addr\":" + String(dupAddr() ? "true" : "false") + "}";
    return j;
}

String FunkLink::statusJson() const {
    if (!active()) return "{\"role\":null}";
    uint32_t now = millis();
    String j = "{\"role\":\"" + String(funkRoleName(role_)) + "\"";
    j += ",\"name\":\"" + jsonText(name_) + "\"";
    j += ",\"addr_nr\":" + String(crsfAddr_ >= 0xC0 ? crsfAddr_ - 0xC0 : 0);
    j += ",\"addr\":\"" + hexAddr(crsfAddr_) + "\",\"mac\":\"" + macText(myMac_) + "\"";
    j += ",\"id\":" + String(cfg_.id) + ",\"ch\":" + String(cfg_.ch) + ",\"pwr\":" + String(cfg_.pwr);
    j += ",\"rate\":" + String(cfg_.rateMs) + ",\"back\":" + String(cfg_.back ? "true" : "false");
    j += ",\"connected\":" + String(connected() ? "true" : "false");
    uint32_t lostMs = lastPartnerMs_ ? now - lastPartnerMs_ : now - startMs_;
    j += ",\"lost_ms\":" + String(lostMs);
    j += ",\"loss\":" + String(worstLoss(), 1) + ",\"rssi\":" + String(worstRssi());
    j += ",\"conflict\":" + String(conflict() ? "true" : "false");
    j += ",\"dup_addr\":" + String(dupAddr() ? "true" : "false");
    j += ",\"own\":{\"ch_cable\":" + String(cntLast_.rcOut) + ",\"ch_funk\":" + String(cntLast_.rcIn);
    j += ",\"frames_out\":" + String(cntLast_.framesOut) + ",\"frames_in\":" + String(cntLast_.framesIn);
    j += ",\"sent\":" + String(cntLast_.sent) + ",\"noack\":" + String(noAck_);
    j += ",\"qfull\":" + String(qFull_) + ",\"senderr\":" + String(sendErr_);
    j += ",\"crc\":" + String(crcErr_) + ",\"stale\":" + String(stale_) + ",\"rate_drop\":" + String(rateDrop_) + "}";
    // 1.5.1: Gesamtzaehler seit Start bzw. Zuruecksetzen
    j += ",\"total\":{\"frames_out\":" + String(cnt_.framesOut) + ",\"frames_in\":" + String(cnt_.framesIn);
    j += ",\"sent\":" + String(cnt_.sent) + ",\"wr_bytes\":" + String(wrBytes_) + ",\"wr_resync\":" + String(wrResync_);
    j += ",\"back_drop\":" + String(backDrop_) + ",\"no_peer\":" + String(noPeer_) + "}";
    j += ",\"peers\":[";
    bool first = true;
    for (auto& p : peers_) {
        if (!p.used) continue;
        if (!first) j += ",";
        first = false;
        bool alive = now - p.lastAnyMs <= ALIVE_MS;
        j += "{\"name\":\"" + jsonText(p.name[0] ? p.name : "?") + "\"";
        j += ",\"role\":\"" + String(funkRoleName(p.role)) + "\"";
        j += ",\"partner\":" + String(isPartner(p) ? "true" : "false");
        j += ",\"addr\":\"" + hexAddr(p.crsfAddr) + "\",\"mac\":\"" + macText(p.mac) + "\"";
        j += ",\"pps\":" + String(p.rxSecLast) + ",\"loss\":" + String(peerLoss(p), 1);
        j += ",\"rssi\":" + String(p.rssi) + ",\"alive\":" + String(alive ? "true" : "false");
        j += ",\"drops\":" + String(p.dropouts) + ",\"gap\":" + String(p.maxGapMs);
        j += ",\"up\":\"" + (alive ? upText(now - p.sinceMs) : String("weg")) + "\"}";
    }
    j += "]}";
    return j;
}

void FunkLink::registerWeb(WebServer& server) {
    WebServer* sv = &server;
    server.on("/api/funk", HTTP_GET, [this, sv]() {
        sv->sendHeader("Cache-Control", "no-cache");
        sv->send(200, "application/json", statusJson());
    });
    server.on("/api/funk/reset", HTTP_POST, [this, sv]() {
        resetStats();
        sv->send(200, "application/json", "{\"ok\":true}");
    });
}
#endif
