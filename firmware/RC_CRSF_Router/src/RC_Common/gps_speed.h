/*
 * gps_speed.h  –  RC_Common (Rolle "GPS" auf einem Zusatz-UART, siehe
 * uart_roles.h; urspruenglich aus ESP32-RC-Sound v7)
 *
 * Liest ein NMEA-GPS-Modul (z. B. u-blox NEO-M8N oder ATGM336H) über einen
 * Hardware-UART ein und stellt Ground Speed sowie Positionsdaten bereit.
 *
 * Anschluss an einen Zusatz-UART mit Rolle "GPS" (Pins siehe Projekt):
 *   GPS-TX  -> Port RX  (Empfang der NMEA-Daten)
 *   GPS-RX  <- Port TX  (Senden der Konfiguration an das GPS)
 *
 * Baudrate fest: nach dem Start wird das GPS einmalig auf 5 Hz (9600 Baud)
 * bzw. 10 Hz (ab 38400) und die Ausgabe von GGA+RMC gestellt.
 * Baudrate UART_BAUD_AUTO ("Auto"): der Port sucht die Baudrate selbst,
 * stellt das GPS auf 115200 Baud und 10 Hz und sucht neu, wenn keine Daten
 * mehr kommen (siehe gps_autobaud.h). Befehle gehen fuer CASIC (ATGM336H/
 * AT6558) und u-blox (M8/M9/M10) raus. Die Einstellung ist flüchtig und
 * wird bei jedem Start neu gesetzt.
 *
 * Benötigte Bibliothek: TinyGPSPlus.
 */
#ifndef GPS_SPEED_H
#define GPS_SPEED_H

#include <Arduino.h>

// GPS am UART starten, baud = feste Rate oder UART_BAUD_AUTO
void     gpsSpeedInit(HardwareSerial* serial, uint8_t rxPin, uint8_t txPin, uint32_t baud);
// Bereits geoeffneter Datenstrom, z. B. SoftwareSerial (Soundmodul Basis
// Port C). Fuer "Auto" muss setBaud den Strom auf eine neue Rate stellen;
// maxBaud begrenzt Suche und Umstellung. Ohne setBaud bleibt baud fest.
typedef void (*GpsBaudFn)(uint32_t baud);
void     gpsSpeedInitStream(Stream* stream, uint32_t baud, GpsBaudFn setBaud = nullptr, uint32_t maxBaud = 115200);
bool     gpsIsActive();        // true, sobald ein Port die Rolle GPS hat
bool     gpsBaudAuto();        // Baudrate wird automatisch gesucht
bool     gpsBaudLocked();      // GPS gefunden (bei fester Rate immer true)
uint32_t gpsGetBaud();         // aktuelle Baudrate des GPS-Ports
void     gpsSpeedUpdate();     // NMEA einlesen/auswerten + Konfig nach Boot senden

float    gpsGetSpeedKmh();     // aktuelle Geschwindigkeit in km/h
uint8_t  gpsGetSatellites();   // Anzahl Satelliten im Fix
bool     gpsHasFix();          // gültiger Positions-Fix?

int32_t  gpsGetLatE7();        // Breitengrad * 1e7   (für CRSF-GPS-Frame)
int32_t  gpsGetLonE7();        // Längengrad * 1e7
uint16_t gpsGetHeadingCdeg();  // Kurs über Grund * 100
uint16_t gpsGetAltMeters();    // Höhe in Metern (Frame addiert +1000 selbst)

#endif // GPS_SPEED_H
