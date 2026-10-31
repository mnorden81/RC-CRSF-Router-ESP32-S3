// uart_ports.h - RC_Common: Zusatz-UARTs (Port/UART B und C) starten
//
// Jedes Modul beschreibt seine Zusatz-UARTs als Tabelle (Name, UART, Pins,
// gewaehlte Rolle und Baudrate). uartPortsBegin() startet daraus:
//   CRSF   -> Router der CRSF-Bibliothek (Modulkette), nur wenn Port A CRSF ist
//   S.Port -> sport_lipo (FLVSS/MLVSS-Zellspannungen)
//   GPS    -> gps_speed (bei "Auto" sucht der Port die Baudrate selbst)
//   Raw    -> UART offen, Firmware wertet nichts aus
// S.Port, GPS und Raw laufen nur auf einem Port; doppelt gewaehlt bleibt
// der zweite Port aus. CRSF darf auf beiden Ports laufen (Kette verzweigt).
// Rollen und Baudraten wirken erst nach einem Neustart.
#ifndef RC_UART_PORTS_H
#define RC_UART_PORTS_H

#ifdef ARDUINO
#include <Arduino.h>
#include "uart_roles.h"

class CRSF;

struct UartPort {
    const char*     name;       // Anzeige, z. B. "Port B" oder "UART C"
    HardwareSerial* ser;        // nullptr = Port gibt es auf diesem Board nicht
    uint8_t         rx;
    uint8_t         tx;
    uint8_t         role;       // gewaehlte Rolle (UROLE_*)
    uint32_t        baud;       // gewaehlte Baudrate, UART_BAUD_AUTO = automatisch
    // von uartPortsBegin() gesetzt:
    uint8_t         activeRole; // tatsaechlich gestartete Rolle
    bool            duplicate;  // aus, weil die Rolle schon auf einem anderen Port laeuft
    bool            idle;       // CRSF gewaehlt, aber Port A ist kein CRSF
};

// ports: 1 oder 2 Eintraege (B, C). crsfBus = Port A laeuft mit CRSF.
void uartPortsBegin(UartPort* ports, uint8_t count, CRSF& crsf, bool crsfBus);
// Jeden Loop: S.Port und GPS einlesen (ohne aktive Rolle ohne Wirkung)
void uartPortsUpdate();
// Alle 100 ms CRSF-Telemetrie: Zellspannungen (0x0E) und GPS (0x02). Kein
// Batterie-Frame (0x08), sonst wuerde die vom Empfaenger gemessene RxBt
// ueberschrieben. Nur aufrufen, wenn Port A CRSF ist.
void uartPortsTelemetry(CRSF& crsf);

#endif
#endif
