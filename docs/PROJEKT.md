# CRSF-Router (ESP32-S3)

Stand: 31.10.2026 · Firmware v0.2.0 · Status: Gerüst, nicht kompiliert, nicht geflasht, nicht getestet

## Ziel
Ein ESP32-S3 sitzt an einem freien seriellen Port des ELRS-Empfängers und bündelt daran zwei weitere CRSF-Geräte (z. B. zwei ESP32-Multiswitche mit je eigener CRSF-Adresse). Der Router meldet sich selbst als CRSF-Gerät an. Im Lua-Script des Senders (und später in der WebUI) wird pro Port gewählt, ob dort CRSF oder S.Port (Sensoren) genutzt wird.

Verwandte Projekte: ESC Coordinator (zwei ESCape32-ESCs an einem Port), ESP32 Multiswitch (CRSF), ESP32-GPS-Telemetry (S.Port/CRSF, Web- und Lua-Konfiguration).

## Hardware
- ESP32-S3 (z. B. Waveshare ESP32-S3-Zero oder DevKitC-1)
- Uplink: UART0, vollduplex, 420 kBaud, zum Empfänger
- Port A: UART1, Port B: UART2 (je CRSF oder S.Port; Halbduplex an A/B ist möglich und noch offen)
- Debug über USB-CDC (alle drei Hardware-UARTs bleiben frei)
- Pins in `main.cpp` sind Platzhalter: Uplink RX44/TX43, A RX4/TX5, B RX6/TX7

## Architektur
- `crsf.h`: Konstanten, CRC8 (Poly 0xD5), Frame-Struktur, Byte-Parser, einfache Frame-Queue
- `config.h`: `Config` (Modus und CRSF-Adresse je Port), Laden und Speichern in NVS (Namespace "router")
- `main.cpp`: Parameter-Menü, Geräteanmeldung, Routing, Port-Initialisierung

### Routing
- Nicht-Extended-Frames (RC-Channels, Link-Stats): unverändert an alle Ports im CRSF-Modus
- Extended Frames (ab 0x28): nach Zieladresse an Port A bzw. B
- Broadcast (Ziel 0x00): Router antwortet selbst und leitet an beide CRSF-Ports weiter
- Frames von Port A/B: unverändert über eine Queue zum Empfänger
- Adressen (Platzhalter): Router 0xCA, Multiswitch A 0xC0, Multiswitch B 0xC1. Jedes Gerät braucht eine eindeutige Adresse, sonst müssten Adresse und CRC umgeschrieben werden.

### Parameter-Menü (Lua)
1. Port A Modus: Aus / CRSF / S.Port
2. Port B Modus: Aus / CRSF / S.Port
3. Port A Adresse (nur sichtbar bei CRSF)
4. Port B Adresse (nur sichtbar bei CRSF)
5. Speichern + Neustart (Command)

Änderungen wirken erst nach Speichern und Neustart. Alle Parameter passen in einen Chunk.

## Stand v0.2.0
- Anmeldung per DEVICE_PING/DEVICE_INFO als "CRSF Router"
- Parameter lesen/schreiben (0x2C/0x2D, Antwort 0x2B)
- NVS-Konfiguration, Routing nach gespeicherten Modi und Adressen
- Versionsnummer projektweit konsistent (`platformio.ini` → Startmeldung und DEVICE_INFO)

## Offen
1. WebUI und WLAN (v0.3.0): Aktivierung und Verhalten wie beim ESP32-Multiswitch (Setup-Jumper, AP-Modus). Dafür wird der WLAN/WebUI-Code des Multiswitch benötigt (MultiSwitch_ESP32_TXRX v3.11 oder V3). Gleiches `Config`-Modell wie Lua.
2. S.Port-Treiber: aktuell wird der Port im S.Port-Modus nur invertiert mit 57600 Baud geöffnet. Fehlt: Polling der Sensor-IDs, Sensor-Pool, Umsetzung in CRSF-Telemetrie.
3. Halbduplex-Umschaltung an Port A/B.
4. Pins und Adressen festlegen.
5. Test am echten Gerät: Lua-Laden der Parameter, Command-Handling, Verhalten bei zwei Multiswitchen.

## Hinweise und Arbeitsregeln
- Firmware kann in der Sandbox nicht kompiliert, geflasht oder getestet werden. Geprüft ist nur per Code-Review und Konsistenzchecks.
- Versionsnummer bei jeder Änderung korrekt hochzählen und projektweit konsistent halten.
- Minimale, isolierte, belegte Fixes statt spekulativer Änderungen. Im Zweifel zurück auf einen bestätigt funktionierenden Stand.
- Backups gehören in einen einzigen Unterordner `backups/` im Projektordner und nur für Versionen, die nachweislich funktioniert haben.
- GitHub spiegelt nur den aktuellen Stand, Historie und Backups bleiben auf dem Rechner.

## Dateien
- `platformio.ini`
- `src/main.cpp`
- `src/config.h`
- `src/crsf.h`
