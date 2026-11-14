// bruecke_types.h - Datentypen der Funk-Bruecke (eigene Datei, damit die
// Arduino-IDE ihre automatischen Funktions-Prototypen dahinter einfuegt).
#pragma once
#include <stdint.h>

// Empfangene Pakete gehen aus dem WLAN-Task ueber eine Queue in loop()
struct RxItem { uint8_t mac[6]; uint8_t len; uint8_t data[250]; };

// Statistik je Gegenstelle
struct Peer {
    bool     used;
    uint8_t  mac[6];
    uint16_t lastSeq;
    bool     haveSeq;
    uint32_t lastMs;           // letztes DATA-Paket (CRSF)
    uint32_t lastAnyMs;        // letztes Paket ueberhaupt (auch Ping)
    uint32_t rxSec, rx, lost;  // Pakete laufende Sekunde, gesamt, verloren gesamt
    uint32_t rxSecLast;        // Pakete in der letzten Sekunde
    uint32_t dropouts;         // Luecken > LINK_TIMEOUT_MS
    uint32_t maxGapMs;         // laengste Luecke seit Start
    uint32_t gapSecMax;        // laengste Luecke in der laufenden Sekunde
    uint32_t gapSecLast;       // laengste Luecke in der letzten Sekunde
    uint32_t rttUs, rttMaxUs;  // letzte und groesste Ping-Zeit (hin+zurueck)
    uint32_t rttSumUs, rttCnt; // Mittelwert je Sekunde
    uint32_t rttAvgUs;
    int8_t   rssi;
};
