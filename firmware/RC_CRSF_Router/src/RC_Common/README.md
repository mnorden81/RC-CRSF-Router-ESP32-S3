# RC_Common

Gemeinsame Basis aller ESP32-Firmwaren (RC-Multiswitch C3/S3, Multiswitch Basis V2/V3/TXRX, Soundmodul Basis, Soundmodul Mini S3). Jedes Projekt trägt eine **identische Kopie** dieses Ordners, es gibt keine externe Abhängigkeit:

| Projekt | Ordner |
|---|---|
| RC-Multiswitch-ESP32-C3 / -S3 | `firmware/RC_Multiswitch/src/RC_Common/` |
| ESP32-Multiswitch-Basis V2 / V3 / TXRX | `MultiSwitch_ESP32*/src/RC_Common/` |
| ESP32-Sound-Basis | `src/ESP32-RC-Sound/src/RC_Common/` |
| ESP32-Sound-Mini-S3 | `firmware/src/RC_Common/` |

**Regel:** Änderungen nur gemeinsam in allen Kopien und `RC_COMMON_VERSION` in `rc_common.h` hochzählen. `MANIFEST.sha256` listet die Prüfsummen aller Dateien, die CI prüft jede Kopie damit (`sha256sum -c MANIFEST.sha256` im Ordner). Gleiche Version heißt gleicher Inhalt.

## Inhalt

| Datei | Inhalt |
|---|---|
| `CRSF_ESP32/` | CRSF-Bibliothek 1.1.0 mit Router für die Modulkette (eigene README) |
| `uart_roles.*` | Rollen der Zusatz-UARTs, überall gleich nummeriert: 0 Aus, 1 CRSF, 2 S.Port, 3 GPS, 4 Raw. Baudratenliste mit „Auto“, Umrechnung alter NVS-Werte |
| `uart_ports.*` | Startet Port B/C nach Rolle (CRSF-Router, S.Port, GPS, Raw), erkennt doppelte Rollen, sendet die Telemetrie |
| `sport_lipo.*` | FrSky S.Port Master für FLVSS/MLVSS → CRSF-Zellspannungen |
| `gps_speed.*` | NMEA-GPS → CRSF-GPS-Telemetrie und Geschwindigkeit |
| `gps_autobaud.*` | Baudrate „Auto“ für GPS: Suche 9600/115200/38400/57600/19200, Umstellung auf 115200 und 10 Hz (CASIC/ATGM336H und u-blox M8/M9/M10), neue Suche nach 3 s ohne Daten |
| `lua_menu.*` | Lua-Parametermenü als Tabelle (Vorbild G030/WMuCpp) |
| `settings_common.*` | Gemeinsame NVS-Schlüssel (WLAN, Moduladresse, Failsafe, UART-Rollen) mit einmaliger Übernahme der alten Schlüssel |
| `wifi_failsafe.h` | WLAN schaltet sich ohne RC-Signal nach Timeout ein (nie automatisch aus) |
| `status_led.*` | Blinkmuster: aus, langsam (OK), schnell (Failsafe), Doppelblitz (Konfigurationsfehler); WS2812 oder GPIO |
| `web_common.*` | Gemeinsamer WebUI-Kern: OTA-Upload `/api/otaupdate`, `/rc_common.js` (Rollen, Baudraten, OTA-Upload, Flash-Backups, Fehlertexte), `/api/uartroles` |
| `config_backup.*` | Einstellungs-Backups im Flash (LittleFS, Partition `spiffs`): `/api/backup/list`, `save`, `get`, `delete`, je 5 von Hand und 3 automatisch (vor Import und Firmware-Update). Inhalt wie beim Export als Datei, das Projekt liefert ihn. Nur aktiv, wenn das Projekt `cfgBackupRegister()` aufruft |
| `tests/` | Host-Tests mit Unity, laufen ohne ESP32: `sh RC_Common/tests/run_tests.sh` |

## Gemeinsame NVS-Schlüssel

Im Namensraum des jeweiligen Projekts: `ssid pass wip dnam madr wfauto wftmo wfalways u1role u1baud u2role u2baud rcc`. Fehlt `rcc`, liest die Firmware einmalig die alten Schlüssel (`ip mod_adr wauto watout walways pbrole pcrole gpsbaud pbmode pbbaud pcmode pcbaud pcgps`) und schreibt die neuen. Die alten bleiben stehen, damit eine ältere Firmware nach dem Zurückspielen ihre Werte noch findet.

## Versionen

| RC_Common | CRSF_ESP32 | Änderung |
|---|---|---|
| 1.0.0 | 1.1.0 | Erste gemeinsame Basis (Schritte 1–9 des Firmware-Vergleichs vom 21.10.2026) |
| 1.1.0 | 1.1.0 | GPS-Baudrate „Auto“ (wie Betaflight/INAV): Eintrag 0 der Baudratenliste, Vorgabe für die Rolle GPS. Bei „Auto“ nehmen die anderen Rollen ihre Protokoll-Baudrate. Alte GPS-Einstellungen werden bei der Übernahme auf „Auto“ gesetzt. `rcFillBaud()` in `/rc_common.js` |
| 1.2.0 | 1.1.0 | Einstellungs-Backups im Flash (`config_backup.*`, `rcBackup*()` in `/rc_common.js`). `rcOtaUpload()` nimmt einen optionalen Schritt vor dem Upload (automatisches Backup) |

Voraussetzung: ESP32 Arduino Core 3.x (Multiswitch: arduino-cli `esp32:esp32@3.0.7`, Soundmodule: PlatformIO pioarduino 51.03.07 = Core 3.0.7).
