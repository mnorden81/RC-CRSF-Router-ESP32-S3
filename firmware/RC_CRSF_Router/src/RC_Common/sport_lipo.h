#ifndef SPORT_LIPO_H
#define SPORT_LIPO_H

/*
 * sport_lipo.h  –  RC_Common (Rolle "S.Port" auf einem Zusatz-UART, siehe
 * uart_roles.h; urspruenglich aus ESP32-RC-Sound v7)
 *
 * S.Port Master-Polling fuer bis zu 2 FrSky FLVSS/MLVSS LiPo-Sensoren.
 * Der ESP32 übernimmt die Rolle des S.Port-Masters und pollt die Sensoren
 * aktiv ueber einen Zusatz-UART (Pins siehe Projekt). Die Zellspannungen
 * gehen als CRSF-Telemetrie an den Empfaenger.
 *
 * Schaltung (Half-Duplex, S.Port ist invertierter UART 57600 Baud):
 *
 *   ESP32 UART TX ──[1N4148 Anode→Kathode]──┬── S.Port Signal (gelb)
 *   ESP32 UART RX ───────────────────────────┘
 *   ESP32 GND ──────────────────────────────────────── S.Port GND (schwarz)
 *
 *   Diode 1N4148: Anode an UART TX, Kathode Richtung Bus
 *   (schwarzer Ring zeigt WEG vom ESP32, Richtung S.Port Kabel)
 *
 * Sensor Poll-IDs (Werkseinstellung, je nach Projekt im Web aenderbar):
 *   Sensor 1 (Pack 1): Physical ID 1 (EdgeTX) → Poll-Byte 0xA1
 *   Sensor 2 (Pack 2): Physical ID 2 (EdgeTX) → Poll-Byte 0x22
 *
 * ⚠ 12S Hinweis: Bei 2×6S in Reihe darf GND NUR am ersten Sensor
 *   angeschlossen werden. Am zweiten Sensor entsteht sonst ein Kurzschluss!
 */

#include <Arduino.h>

class CRSF;

// S.Port: 57600 Baud, 8N1, invertiert
#define SPORT_BAUD       57600

// ── Sensor-Konfiguration ──────────────────────────────────────────────
#define SPORT_MAX_CELLS    6     // FLVSS/MLVSS: max. 6S pro Sensor
#define SPORT_NUM_SENSORS  2     // Pack 1 + Pack 2

// Physical Poll-Bytes kommen aus dem Projekt (sportLipoUsePollIds()).
// Standardwerte (Werkseinstellung):
//   Sensor 1: Physical ID 0x02 → Poll-Byte 0xA1
//   Sensor 2: Physical ID 0x03 → Poll-Byte 0x22

// ── Protokoll-Konstanten ──────────────────────────────────────────────
#define SPORT_START_BYTE   0x7E
#define SPORT_DATA_FRAME   0x10
#define SPORT_CELLS_ID     0x0300   // Data-ID FLVSS Einzelzellen (0x0300–0x030F)
#define SPORT_STUFF_BYTE   0x7D     // Byte-Stuffing Escape
#define SPORT_STUFF_MASK   0x20     // XOR-Maske nach Escape

// ── Timing ────────────────────────────────────────────────────────────
#define SPORT_POLL_MS      30       // Poll-Intervall pro Sensor (ms) – gibt Antwortzeit
#define SPORT_TIMEOUT_MS   2000     // Sensor offline nach dieser Zeit ohne Paket

// ── Diagnose ──────────────────────────────────────────────────────────
// SPORT_DEBUG_RAW = 1 : jede Sekunde RX-Byte-Zähler + Hexdump auf Serial,
// zusaetzlich [CELL]-, [S.Port LIVE]- und Sensor-Offline-Meldungen.
// Fuer Normalbetrieb auf 0. Zum Debuggen auf 1 setzen.
#define SPORT_DEBUG_RAW    0

// SPORT_DEBUG_PROBE = 1 : Diagnose-Sweep (deaktiviert, war fehlerhaft).
#define SPORT_DEBUG_PROBE  0
#define SPORT_PROBE_PHASE_MS 5000   // Dauer pro ID-Phase im Sweep (ms)

// ── Datenstruktur pro Sensor ──────────────────────────────────────────
struct LipoSensorData {
    float    cellVoltage[SPORT_MAX_CELLS];  // Einzelzellen in Volt
    uint8_t  cellCount;                     // Erkannte Zellanzahl (0 = offline)
    float    totalVoltage;                  // Summe aller Zellen in Volt
    float    minCell;                       // Niedrigste Einzelzelle in Volt
    unsigned long lastUpdateMs;             // Zeitstempel letztes gültiges Paket
    bool     online;                        // true = Sensor antwortet
};

// ── Öffentliche Daten ─────────────────────────────────────────────────
extern LipoSensorData lipoSensor[SPORT_NUM_SENSORS];

// ── Öffentliche Funktionen ────────────────────────────────────────────
// Aufruf aus uartPortsBegin() (uart_ports.h) fuer den Port mit Rolle S.Port.
void    sportLipoInit(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);
// Zeiger auf die zwei Poll-IDs des Projekts (werden live gelesen, Aenderung
// im Web wirkt sofort). Ohne Aufruf gelten 0xA1/0x22.
void    sportLipoUsePollIds(const uint8_t* ids2);
void    sportLipoUpdate();              // Non-blocking, in loop() aufrufen
bool    sportLipoIsActive();            // true = UART initialisiert (ein Port hat die Rolle S.Port)

float   sportGetTotalVoltage();         // Gesamtspannung beider Packs (V)
float   sportGetMinCell();              // Niedrigste Einzelzelle aller Packs (V)
uint8_t sportGetTotalCells();           // Gesamtzellanzahl beider Packs
uint8_t sportCalcSoC(float minCell);    // SoC aus min. Zellspannung (0–100%)
void    sportSendCellsTelemetry(CRSF& crsf, uint8_t sensorIdx); // CRSF Voltages senden

// ── Diagnose-Zaehler (immer aktiv, nicht nur bei SPORT_DEBUG_RAW) ──────
// Uebernommen aus ESP32-GPS-Telemetry: granulare Zaehler machen "geht
// nicht" auf der Debug-Seite direkt sichtbar (kommen ueberhaupt Bytes an?
// werden Polls beantwortet? passt die CRC?), statt nur online/offline.
uint32_t sportGetRawBytesRx();      // alle je empfangenen Bytes auf dem S.Port-Bus
uint32_t sportGetPollsSent();       // von uns gesendete Polls (0x7E + Poll-ID)
uint32_t sportGetValidFrames();     // erfolgreich dekodierte CELLS-Frames
uint32_t sportGetCrcErrors();       // CRC-Fehler in Sensor-Antworten
uint32_t sportGetNoResponseCount(); // Poll gesendet, aber keine/ungueltige Antwort (kein DATA_FRAME)
uint8_t  sportGetLastPollId();      // zuletzt gesehene Poll-ID (eigene oder fremde Antwort)

#endif // SPORT_LIPO_H
