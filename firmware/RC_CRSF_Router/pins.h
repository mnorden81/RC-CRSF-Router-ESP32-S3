#ifndef PINS_H
#define PINS_H

/*
 * pins.h  -  CRSF-Router (ESP32-S3-Zero)
 *
 * Belegung (v0.3.5, an Martins Board geprueft): Port A (Eingang) RX44/TX43
 * (rechte Leiste oben, TX/RX), Port B 5/6 (links unten), Port C 7/8 (rechts
 * unten). GPIO14/15/16 sind beim S3-Zero nur Loetpads. Auswahlregeln fuer den ESP32-S3:
 *   - GPIO 26-37 sind beim S3FH4R2 durch Flash/PSRAM belegt (tabu)
 *   - GPIO 0, 45, 46 sind Strapping-Pins; GPIO 0 ist die BOOT-Taste und
 *     wird erst nach dem Start als WLAN-Taste gelesen
 *   - GPIO 19/20 sind der native USB-Anschluss (Konsole)
 *   - GPIO 21 ist beim S3-Zero die RGB-LED (WS2812)
 */

// ── Port A = Eingang/Uplink zum Empfaenger: CRSF, Hardware-UART0 (Serial0)
// IO43/44 sind die TX/RX-Pins am Header (die Konsole laeuft ueber USB).
// Das ROM-Bootlog auf IO43 (nur beim Start, 115200) verwirft der Empfaenger
// per CRC.
#define PIN_UPLINK_RX  44
#define PIN_UPLINK_TX  43

// ── Port B: Hardware-UART1 (Serial1), Rolle waehlbar ─────────────────
// S.Port-Rolle: TX -[1N4148 Anode->Kathode]-> S.Port-Signal <- RX
#define PIN_PORTB_RX   5
#define PIN_PORTB_TX   6

// ── Port C: Hardware-UART2 (Serial2), Rolle waehlbar ─────────────────
#define PIN_PORTC_RX   7
#define PIN_PORTC_TX   8

// ── Status-LED: RGB-LED (WS2812) des S3-Zero ─────────────────────────
#define PIN_STATUS_LED 21

// ── BOOT-Taste (GPIO0): innerhalb von 3 s nach dem Start druecken = WLAN an
#define PIN_BOOT_BUTTON 0
#define WIFI_BUTTON_WINDOW_MS 3000
// Im Betrieb: BOOT 2 s halten = WLAN an/aus
#define WIFI_BUTTON_HOLD_MS   2000

// ── Empfaengerspannung: Spannungsteiler an ADC1_CH0 ──────────────────
//   +V (Empfaenger) ── 33 kOhm ──┬── GPIO1
//                                └── 10 kOhm ── GND
// Teiler 4,3 : 1 -> bis ca. 13 V messbar. Ohne Teiler "Spannung senden"
// auf Aus lassen (offener Pin liefert Zufallswerte).
#define PIN_VSENSE       1
#define VSENSE_R_TOP     33.0f
#define VSENSE_R_BOTTOM  10.0f

#endif // PINS_H
