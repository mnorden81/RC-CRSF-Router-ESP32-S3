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
| `CRSF_ESP32/` | CRSF-Bibliothek 1.2.0 mit Router für die Modulkette (eigene README) |
| `uart_roles.*` | Rollen der Zusatz-UARTs, überall gleich nummeriert: 0 Aus, 1 CRSF, 2 S.Port, 3 GPS, 4 Raw. Baudratenliste mit „Auto“, Umrechnung alter NVS-Werte |
| `uart_ports.*` | Startet Port B/C nach Rolle (CRSF-Router, S.Port, GPS, Raw), erkennt doppelte Rollen, sendet die Telemetrie |
| `sport_lipo.*` | FrSky S.Port Master für FLVSS/MLVSS → CRSF-Zellspannungen |
| `gps_speed.*` | NMEA-GPS → CRSF-GPS-Telemetrie und Geschwindigkeit |
| `gps_autobaud.*` | Baudrate „Auto“ für GPS: Suche 9600/115200/38400/57600/19200, Umstellung auf 115200 und 10 Hz (CASIC/ATGM336H und u-blox M8/M9/M10), neue Suche nach 3 s ohne Daten |
| `lua_menu.*` | Lua-Parametermenü als Tabelle (Vorbild G030/WMuCpp), Felder per `view()` umbenennbar, sperrbar und ausblendbar |
| `settings_common.*` | Gemeinsame NVS-Schlüssel (WLAN, Moduladresse, Failsafe, UART-Rollen) mit einmaliger Übernahme der alten Schlüssel |
| `wifi_failsafe.h` | WLAN schaltet sich ohne RC-Signal nach Timeout ein (nie automatisch aus) |
| `status_led.*` | Status-LED, gleiches Schema in allen Modulen (seit 1.4.0). RGB-LED: grün langsam = OK, rot schnell = Failsafe/kein RC-Link, rot Aufblitzen = kein Signal, blau Doppelblitz = Adresskonflikt, gelb Doppelblitz = sonstiger Konfigurationsfehler, violett statt grün/rot = WLAN an, blau blinkend = BOOT-Fenster. Farbfolge RGB/GRB umschaltbar. Einfache LED: langsam, schnell, aus, Doppelblitz |
| `web_common.*` | Gemeinsamer WebUI-Kern: OTA-Upload `/api/otaupdate`, `/rc_common.js` (Rollen, Baudraten, OTA-Upload, Flash-Backups, Fehlertexte), `/api/uartroles` |
| `config_backup.*` | Einstellungs-Backups im Flash (LittleFS, Partition `spiffs`): `/api/backup/list`, `save`, `get`, `delete`, je 5 von Hand und 3 automatisch (vor Import und Firmware-Update). Inhalt wie beim Export als Datei, das Projekt liefert ihn. Nur aktiv, wenn das Projekt `cfgBackupRegister()` aufruft |
| `funk_link.*` | Module per Funk verbinden (ESP-NOW, seit 1.5.0, nur ESP32). `FunkLink` ist ein `Stream`: die Zentrale (Modul am Empfänger) hängt ihn als Router-Port D an (`crsf.attach_funk_stream()`), ein Teilnehmer liest CRSF daraus statt vom Kabel (`crsf.attach_stream()`). Paketformat wie Funk-Brücke v0.1.9, Unicast mit Quittung, Ping mit Name, Rolle und CRSF-Adresse. `/api/funk`, `/api/funk/reset`, NVS `fkid fkch fkpwr fkrate fkback` |
| `tests/` | Host-Tests mit Unity, laufen ohne ESP32: `sh RC_Common/tests/run_tests.sh` |

## Gemeinsame NVS-Schlüssel

Im Namensraum des jeweiligen Projekts: `ssid pass wip dnam madr wfauto wftmo wfalways u1role u1baud u2role u2baud rcc`. Fehlt `rcc`, liest die Firmware einmalig die alten Schlüssel (`ip mod_adr wauto watout walways pbrole pcrole gpsbaud pbmode pbbaud pcmode pcbaud pcgps`) und schreibt die neuen. Die alten bleiben stehen, damit eine ältere Firmware nach dem Zurückspielen ihre Werte noch findet.

## Versionen

| RC_Common | CRSF_ESP32 | Änderung |
|---|---|---|
| 1.0.0 | 1.1.0 | Erste gemeinsame Basis (Schritte 1–9 des Firmware-Vergleichs vom 21.10.2026) |
| 1.1.0 | 1.1.0 | GPS-Baudrate „Auto“ (wie Betaflight/INAV): Eintrag 0 der Baudratenliste, Vorgabe für die Rolle GPS. Bei „Auto“ nehmen die anderen Rollen ihre Protokoll-Baudrate. Alte GPS-Einstellungen werden bei der Übernahme auf „Auto“ gesetzt. `rcFillBaud()` in `/rc_common.js` |
| 1.2.0 | 1.1.0 | Einstellungs-Backups im Flash (`config_backup.*`, `rcBackup*()` in `/rc_common.js`). `rcOtaUpload()` nimmt einen optionalen Schritt vor dem Upload (automatisches Backup) |
| 1.2.1 | 1.1.0 | Lua: lange Auswahlfelder (über 55 Byte) werden Chunk für Chunk beantwortet, wie vom Sender angefragt. Außerdem hatte jeder Chunk ein überzähliges Byte vor der CRC. Vorher kam immer Chunk 0, solche Felder und ihr Ordner erschienen im ELRS-Lua nicht. Zuerst im CRSF-Router S3 (v0.3.4), die anderen Kopien folgen |
| 1.2.2 | 1.1.1 | Adresskonflikt mit Port: `getDuplicatePorts()` (Bit 0 = B, Bit 1 = C), rein additiv. Zuerst im CRSF-Router S3 (v0.3.6), die anderen Kopien folgen |
| 1.3.0 | 1.1.1 | Lua: Felder ausblenden. `LuaView.hidden` (in `view()` setzen) schickt das Feld mit Bit 7 im Datentyp (CRSF „hidden“), `CRSF::setNextParamHidden()`. Typ und Feld-ID bleiben, das ELRS-Lua lädt das Feld nach Änderungen im selben Ordner neu und blendet es wieder ein. Ein Lua ohne diese Kennung zeigt alles wie bisher. Enthält 1.2.1 und 1.2.2. Außerdem laufen CRSF_ESP32 und `lua_menu.*` ohne ESP32 (RP2350): `init_crsf()`/`init_router()` und `StatusLed` nur bei `ESP32`, `<initializer_list>` in `crsf_platform.h`. Der RP2350-Router trägt damit einen unveränderten Auszug. Zuerst im RC-Multiswitch-Sound-S3 (v1.05/v1.06), dann in allen Kopien |
| 1.4.0 | 1.1.1 | Einheitliche Status-LED: `statusLedLook()` liefert Farbe und Muster (Farben wie beim CRSF-Router, „kein Signal“ jetzt rot Aufblitzen 100/900 ms statt aus), `StatusLed::update(mode, now, dupAddress, wifiOn)`, `setRgbOrder()` für LEDs mit RGB-Folge (ESP32-S3-Zero), `showBootWindow()`/`statusLedBootOn()` für das BOOT-Fenster. Einfache LEDs unverändert. Zuerst im RC-Multiswitch-Sound-S3, gleichzeitig in allen Kopien |

| 1.5.2 | 1.2.1 | CRSF_ESP32 1.2.1: WM-Kommandos unabhängig von der Zieladresse (Modul-Adresse ungleich 0 schaltete per Widget nicht mehr) |
| 1.5.1 | 1.2.0 | `funk_link`: Gesamtzähler in `/api/funk` (`total`: Frames raus/rein, gesendet, geschriebene Bytes, verworfen ohne Gegenstelle, Rückkanal aus, Rahmenfehler) für die Fehlersuche am Rückweg des Teilnehmers |
| 1.5.0 | 1.2.0 | Funk-Verbindung `funk_link.*` (ESP-NOW, aus der Funk-Brücke v0.1.9). CRSF_ESP32 1.2.0: Router-Port D für den Funk (`attach_funk_stream()`, `getDuplicatePorts()` Bit 2). Zuerst im RC-Multiswitch v5.01 und im CRSF-Router S3 v0.4.0, die anderen Kopien folgen |

Voraussetzung: ESP32 Arduino Core 3.x (Multiswitch: arduino-cli `esp32:esp32@3.0.7`, Soundmodule: PlatformIO pioarduino 51.03.07 = Core 3.0.7).
