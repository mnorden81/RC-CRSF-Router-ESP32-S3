// funk_link.h - RC_Common: Module per Funk verbinden (ESP-NOW, seit 1.5.0)
//
// Aus der Funk-Bruecke v0.1.9 (CRSF-Router S3, pruefstand/Funk_Bruecke)
// uebernommen und als Datenstrom (Stream) verpackt, damit die CRSF-
// Bibliothek den Funk wie ein Kabel benutzt:
//
//   Zentrale    Modul am Empfaenger. CRSF kommt wie bisher am Kabel
//               (UART A) an und geht zusaetzlich in den Funk:
//               crsf.attach_funk_stream(&funk) (Port D des Routers).
//   Teilnehmer  Modul ohne Kabel zum Empfaenger. CRSF kommt aus dem Funk:
//               crsf.attach_stream(&funk) statt init_crsf().
//
// Alle Teile mit derselben Fahrzeug-Kennung und demselben Funkkanal bilden
// ein Netz. Daten gehen direkt (Unicast mit Quittung und Wiederholung durch
// den Funkchip) an jede bekannte Gegenstelle, ein Rundruf-Ping alle 200 ms
// meldet Name, Rolle und CRSF-Adresse. Eine Zentrale spricht nur mit
// Teilnehmern und Bruecken, ein Teilnehmer nur mit Zentrale und Bruecken.
// Das Paketformat ist dasselbe wie bei der Funk-Bruecke (Version 1), eine
// Bruecke kann also auf einer der beiden Seiten stehen.
//
// Failsafe: kommt nichts mehr aus dem Funk, liefert der Teilnehmer keine
// CRSF-Frames mehr. Das Modul geht dann in seinen eigenen Failsafe wie bei
// abgezogenem Kabel.
//
// WLAN: Funk und Access Point teilen sich das Funkteil und muessen auf
// demselben Kanal laufen. Das Projekt startet den AP mit
// WiFi.softAP(ssid, pass, funk.channel()) und ruft danach (auch nach dem
// Abschalten) funk.wifiChanged() auf. WiFi.mode() muss WIFI_AP_STA sein,
// solange der Funk aktiv ist.
//
// Nur ESP32 (Arduino Core 3.x).
#ifndef RC_FUNK_LINK_H
#define RC_FUNK_LINK_H

#include <stdint.h>

// Rolle (Konstanten statt enum, siehe Hinweis in uart_roles.h)
static constexpr uint8_t FUNK_AUS        = 0;
static constexpr uint8_t FUNK_ZENTRALE   = 1;
static constexpr uint8_t FUNK_TEILNEHMER = 2;
static constexpr uint8_t FUNK_BRUECKE    = 3;   // nur als Gegenstelle

struct FunkConfig {
    uint8_t id     = 1;     // Fahrzeug-Kennung 1..255
    uint8_t ch     = 1;     // Funkkanal 1..13
    uint8_t pwr    = 20;    // Sendeleistung dBm (2..20)
    uint8_t rateMs = 10;    // Zentrale: Kanal-Frames hoechstens alle n ms (0 = alle)
    bool    back   = true;  // Teilnehmer: Rueckkanal (Lua, Telemetrie)
};

// Auswahllisten fuer Lua/WebUI
static constexpr uint8_t FUNK_PWR_COUNT  = 6;
static constexpr uint8_t FUNK_RATE_COUNT = 5;
extern const uint8_t FUNK_PWR_VALUES[FUNK_PWR_COUNT];    // 2 5 8 11 15 20
extern const uint8_t FUNK_RATE_VALUES[FUNK_RATE_COUNT];  // 0 5 10 20 50
#define FUNK_PWR_OPTIONS  "2 dBm;5 dBm;8 dBm;11 dBm;15 dBm;20 dBm"
#define FUNK_RATE_OPTIONS "jedes Frame;5 ms;10 ms;20 ms;50 ms"
uint8_t funkPwrIndex(uint8_t dbm);
uint8_t funkRateIndex(uint8_t ms);
void    funkConfigClamp(FunkConfig& c);
const char* funkRoleName(uint8_t role);

#if defined(ARDUINO) && defined(ESP32)
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>

#define FUNK_MAX_PEERS  8
#define FUNK_RING       1024   // Empfangspuffer (Bytes) fuer die CRSF-Seite

// NVS-Schluessel fkid fkch fkpwr fkrate fkback im Namensraum des Projekts
void funkConfigLoad(Preferences& p, FunkConfig& c);
void funkConfigSave(Preferences& p, const FunkConfig& c);

class FunkLink : public Stream {
public:
    // Funk starten. role FUNK_AUS = nichts tun. name = Name in der
    // Gegenstellen-Liste (max. 16 Zeichen), crsfAddr = eigene CRSF-Adresse.
    bool begin(uint8_t role, const FunkConfig& cfg, const char* name, uint8_t crsfAddr);
    // Jeden Loop VOR crsf.read_packets() aufrufen: Empfang, Ping, Statistik
    void loop();
    // Nach WiFi.softAP()/softAPdisconnect(): Kanal und Sendeleistung neu setzen
    void wifiChanged();

    void setName(const char* name);
    void setCrsfAddr(uint8_t a) { crsfAddr_ = a; }

    bool    active()   const { return role_ != FUNK_AUS; }
    uint8_t role()     const { return role_; }
    uint8_t channel()  const { return cfg_.ch; }
    const FunkConfig& config() const { return cfg_; }
    bool connected() const;      // Gegenstelle in den letzten 500 ms gehoert
    bool conflict()  const;      // Zentrale hoert eine zweite Zentrale
    bool dupAddr()   const;      // Gegenstelle mit derselben CRSF-Adresse
    uint8_t partnerCount() const;
    float   worstLoss() const;   // Verlust in %, schlechteste Gegenstelle
    int8_t  worstRssi() const;

    String summaryJson() const;  // fuer /api/status ("null" ohne Funk)
    String statusJson()  const;  // fuer /api/funk
    void   resetStats();
    // GET /api/funk, POST /api/funk/reset
    void   registerWeb(WebServer& server);

    // Stream (CRSF-Seite)
    int    available() override;
    int    read() override;
    int    peek() override;
    size_t write(uint8_t b) override;
    size_t write(const uint8_t* buf, size_t len) override;
    int    availableForWrite() override { return 512; }
    void   flush() override {}

    struct Peer {
        bool     used;
        bool     pinged;        // Ping empfangen (Name/Rolle bekannt)
        bool     espAdded;
        uint8_t  mac[6];
        char     name[17];
        uint8_t  role;          // FUNK_* (alte Bruecke = FUNK_BRUECKE)
        uint8_t  crsfAddr;      // 0 = unbekannt
        int8_t   rssi;
        bool     haveSeq;
        uint16_t lastSeq;
        uint32_t lastMs, lastAnyMs, sinceMs;
        uint32_t rx, lost, rxSec, rxSecLast, dropouts, maxGapMs;
    };

    // intern (Callback aus dem WLAN-Task)
    void onRecvRaw(const uint8_t* mac, const uint8_t* data, int len, int8_t rssi);
    void onSentRaw(bool ok, bool unicast);

private:
    struct Counters { uint32_t framesOut, framesIn, rcOut, rcIn, sent; };
    void handle(const uint8_t* mac, const uint8_t* data, int len, int8_t rssi);
    void sendPing();
    void sendFrame(const uint8_t* f, uint8_t n);
    bool isPartner(const Peer& p) const;
    bool reachable(const Peer& p, uint32_t now) const;
    Peer* findPeer(const uint8_t* mac, bool create);
    void peerRegister(Peer& p);
    void everySecond();
    void ringPut(const uint8_t* f, uint8_t n);

    uint8_t    role_ = FUNK_AUS;
    FunkConfig cfg_;
    char       name_[17] = {0};
    uint8_t    crsfAddr_ = 0;
    uint8_t    myMac_[6] = {0};
    void*      rxQueue_ = nullptr;
    Peer       peers_[FUNK_MAX_PEERS] = {};
    uint16_t   txSeq_ = 0;
    uint32_t   lastPingMs_ = 0, lastSecMs_ = 0, lastRcSentMs_ = 0, startMs_ = 0;
    uint32_t   lastPartnerMs_ = 0;
    Counters   cnt_ = {}, cntSec_ = {}, cntLast_ = {};
    volatile uint32_t noAck_ = 0, qFull_ = 0;
    uint32_t   sendErr_ = 0, crcErr_ = 0, stale_ = 0, rateDrop_ = 0;
    // 1.5.1: Gesamtzaehler fuer die Fehlersuche (Rueckweg Teilnehmer)
    uint32_t   wrBytes_ = 0, wrResync_ = 0, backDrop_ = 0, noPeer_ = 0;
    // CRSF-Seite: Empfangsring und Sende-Zusammenbau
    uint8_t    ring_[FUNK_RING];
    uint16_t   rHead_ = 0, rCount_ = 0;
    uint8_t    tx_[64];
    uint8_t    txPos_ = 0;
};
#endif

#endif
