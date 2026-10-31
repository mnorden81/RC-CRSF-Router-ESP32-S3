// uart_roles.h - RC_Common: Rollen der Zusatz-UARTs (Port/UART B und C)
//
// Eine Liste fuer alle Module. Die Nummer ist zugleich der Wert im NVS, im
// Web-Dropdown und im Lua-Menue (TEXT_SELECTION-Index). Neue Rollen nur
// hinten anhaengen, damit gespeicherte Werte gueltig bleiben.
//
// Ohne Arduino-Abhaengigkeit, laeuft auch in den Host-Tests (tests/).
#ifndef RC_UART_ROLES_H
#define RC_UART_ROLES_H

#include <stdint.h>

// Bewusst Konstanten statt enum: der Arduino-Prototyp-Generator setzt
// Funktionsprototypen vor eigene Typen, ein enum als Parameter- oder
// Rueckgabetyp im .ino baut dann nicht.
static constexpr uint8_t UROLE_OFF   = 0;   // Port aus
static constexpr uint8_t UROLE_CRSF  = 1;   // Modulkette (CRSF-Router), nur wenn Port A CRSF ist
static constexpr uint8_t UROLE_SPORT = 2;   // FrSky S.Port Master (FLVSS/MLVSS LiPo-Zellen)
static constexpr uint8_t UROLE_GPS   = 3;   // NMEA-GPS (CRSF-GPS-Telemetrie)
static constexpr uint8_t UROLE_RAW   = 4;   // UART offen, Firmware wertet nichts aus
static constexpr uint8_t UROLE_COUNT = 5;

// Anzeigename ("Aus" auch fuer ungueltige Werte)
const char* uartRoleName(uint8_t role);
// ";"-getrennte Liste aller Rollen in Nummernreihenfolge, fuer Lua und Web
const char* uartRoleOptions();
// Ungueltige/alte Werte -> UROLE_OFF
uint8_t     uartRoleClamp(int raw);
// Vorschlag beim Wechsel der Rolle: Protokoll-Baudrate, bei GPS
// UART_BAUD_AUTO (0 = keine, Baudrate unveraendert)
uint32_t    uartRoleDefaultBaud(uint8_t role);
// Rolle darf nur auf einem Port laufen (S.Port, GPS, Raw haben einen Zustand)
bool        uartRoleSingleInstance(uint8_t role);

// Zwei Ports mit Rolle: gleiche Einzel-Rolle auf beiden -> zweiter Port aus.
// Liefert die Rolle fuer den zweiten Port; *duplicate = true, wenn sie
// deshalb abgeschaltet wurde.
uint8_t     uartRoleResolveSecond(uint8_t first, uint8_t second, bool* duplicate);

// Baudraten-Auswahl fuer das Lua-Menue (TEXT_SELECTION, 1 Byte) und das
// Web-Dropdown. Eintrag 0 ist "Auto" (gespeichert als UART_BAUD_AUTO):
// GPS sucht die Baudrate selbst (gps_autobaud.h), die anderen Rollen nehmen
// ihre Protokoll-Baudrate (uartRoleDefaultBaud).
static constexpr uint32_t UART_BAUD_AUTO = 1;   // keine echte Baudrate
static constexpr uint8_t UART_BAUD_COUNT = 8;
extern const uint32_t UART_BAUD_LIST[UART_BAUD_COUNT];
const char* uartBaudOptions();
// Index des passenden oder naechstgelegenen Eintrags ("Auto" nur exakt)
uint8_t     uartBaudToIndex(uint32_t baud);
uint32_t    uartBaudFromIndex(uint8_t index);
// Gespeicherter Wert gueltig? (UART_BAUD_AUTO oder 1200..2000000)
bool        uartBaudValid(uint32_t baud);
// Baudrate, mit der ein Port startet: "Auto" -> Protokoll-Baudrate der
// Rolle, bei GPS die erste Rate der Suche (9600)
uint32_t    uartBaudStart(uint8_t role, uint32_t baud);

// ── Umrechnung alter NVS-Werte (Nummern vor RC_Common 1.0.0) ─────────────
// Soundmodul Basis bis v7.20: 0 Aus, 1 CRSF, 2 S.Port, 3 Hott (nie
// umgesetzt), 4 Raw, 5 GPS
uint8_t uartRoleFromSoundBasisLegacy(int legacy);
// Soundmodul Mini bis v0.02: 0 Aus, 1 CRSF, 2 S.Port, 3 Raw, 4 GPS
uint8_t uartRoleFromSoundMiniLegacy(int legacy);
// RC-Multiswitch bis v4.04: 0 Aus, 1 CRSF, 2 S.Port, 3 GPS (gleich)
uint8_t uartRoleFromMultiswitchLegacy(int legacy);

#endif
