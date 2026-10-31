// gps_autobaud.h - RC_Common: Baudrate eines NMEA-GPS selbst finden
//
// Wie bei Betaflight/INAV: Der Port probiert nacheinander die ueblichen
// Baudraten, bis zwei NMEA-Saetze mit gueltiger Pruefsumme ankommen. Liegt
// die gefundene Rate unter dem Ziel (115200, bei SoftwareSerial weniger),
// wird das GPS umgestellt und die neue Rate geprueft; klappt das nicht,
// bleibt es bei der gefundenen Rate. Kommen spaeter 3 s lang keine
// gueltigen Saetze mehr, beginnt die Suche von vorn.
//
// Reine Ablaufsteuerung ohne UART, damit sie in den Host-Tests laeuft. Das
// Senden der Befehle und das Umschalten des UARTs macht gps_speed.cpp.
#ifndef RC_GPS_AUTOBAUD_H
#define RC_GPS_AUTOBAUD_H

#include <stdint.h>

// Rueckgabe von GpsAutobaud::poll()
static constexpr uint8_t GPSB_IDLE   = 0;   // nichts zu tun
static constexpr uint8_t GPSB_TRY    = 1;   // UART auf baud() stellen (naechster Versuch)
static constexpr uint8_t GPSB_SWITCH = 2;   // Umstellbefehl auf target() senden, dann UART auf baud()
static constexpr uint8_t GPSB_LOCKED = 3;   // UART auf baud(), GPS gefunden: Messrate einstellen
static constexpr uint8_t GPSB_LOST   = 4;   // keine Saetze mehr, UART auf baud(), Suche beginnt neu

// Pruefzeiten in ms
static constexpr uint32_t GPSB_TRY_MS    = 1200;  // je Baudrate bei der Suche
static constexpr uint32_t GPSB_VERIFY_MS = 2000;  // nach dem Umstellen
static constexpr uint32_t GPSB_LOST_MS   = 3000;  // ohne gueltigen Satz -> neue Suche
static constexpr uint8_t  GPSB_NEED      = 2;     // gueltige Saetze fuer "gefunden"

// Prueft NMEA-Saetze ($...*hh) Zeichen fuer Zeichen
class NmeaChecker {
public:
    // true, wenn mit diesem Zeichen ein Satz mit korrekter Pruefsumme endet
    bool feed(uint8_t c);
    void reset() { state = 0; }
private:
    uint8_t state = 0;   // 0 warten auf '$', 1 Inhalt, 2/3 Pruefsummen-Ziffern
    uint8_t sum = 0;
    uint8_t got = 0;
    uint8_t len = 0;
};

class GpsAutobaud {
public:
    // maxBaud begrenzt Suche und Ziel (SoftwareSerial: 38400)
    void begin(uint32_t now, uint32_t maxBaud = 115200);
    // Jedes empfangene Zeichen
    void feed(uint8_t c);
    uint8_t poll(uint32_t now);

    uint32_t baud() const   { return cur; }
    uint32_t target() const { return tgt; }
    bool     locked() const { return st == ST_LOCKED; }
    // Gefundene Rate liess sich nicht erhoehen (GPS nimmt keine Befehle an)
    bool     switchFailed() const { return failed; }

private:
    static constexpr uint8_t ST_SCAN = 0, ST_VERIFY = 1, ST_LOCKED = 2;
    bool     nextScanBaud();
    NmeaChecker nmea;
    uint8_t  st = ST_SCAN;
    uint8_t  idx = 0;        // Position in der Suchliste
    uint8_t  valid = 0;      // gueltige Saetze im aktuellen Fenster
    uint32_t since = 0;      // Beginn des Fensters bzw. letzter gueltiger Satz
    uint32_t cur = 9600;
    uint32_t found = 9600;   // bei der Suche gefundene Rate
    uint32_t tgt = 115200;
    uint32_t maxB = 115200;
    bool     failed = false;
    bool     started = false;
};

// Suchreihenfolge (haeufigste zuerst: Werkseinstellung 9600, nach einem
// Neustart des ESP ohne Stromausfall laeuft das GPS noch mit 115200)
static constexpr uint8_t GPSB_SCAN_COUNT = 5;
extern const uint32_t GPSB_SCAN_LIST[GPSB_SCAN_COUNT];

// ── Befehle an das GPS (fertige Bytes, gps_speed.cpp sendet sie) ─────────
// Jeder Befehl wird fuer beide ueblichen Chipsaetze gesendet; ein Modul
// ignoriert die Befehle des anderen.
//   CASIC (ATGM336H/AT6558):  $PCAS01 Baudrate, $PCAS02 Messrate, $PCAS03 Saetze
//   u-blox M8/M9:             UBX-CFG-PRT, UBX-CFG-RATE, UBX-CFG-MSG
//   u-blox M10:               UBX-CFG-VALSET (RAM)
// Rueckgabe: Laenge in out (0 = Baudrate nicht unterstuetzt / zu klein)
uint16_t gpsCmdCasicBaud(uint32_t baud, char* out, uint16_t max);
uint16_t gpsCmdCasicNmea(const char* body, char* out, uint16_t max);   // "$body*CS\r\n"
uint16_t gpsCmdUbx(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t len, uint8_t* out, uint16_t max);
uint16_t gpsCmdUbxBaud(uint32_t baud, uint8_t* out, uint16_t max);       // CFG-PRT UART1, Ausgabe nur NMEA
uint16_t gpsCmdUbxBaudM10(uint32_t baud, uint8_t* out, uint16_t max);    // CFG-VALSET UART1-BAUDRATE
uint16_t gpsCmdUbxRate(uint16_t ms, uint8_t* out, uint16_t max);         // CFG-RATE
uint16_t gpsCmdUbxRateM10(uint16_t ms, uint8_t* out, uint16_t max);      // CFG-VALSET RATE-MEAS
uint16_t gpsCmdUbxMsgOff(uint8_t nmeaId, uint8_t* out, uint16_t max);    // CFG-MSG NMEA-Satz aus
// Messintervall passend zur Baudrate: ab 38400 100 ms (10 Hz), sonst 200 ms
uint16_t gpsRateForBaud(uint32_t baud);

#endif
