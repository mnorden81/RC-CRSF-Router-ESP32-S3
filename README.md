# RC-CRSF-Router-ESP32-S3

CRSF-Verteiler für ELRS-Empfänger auf einem **ESP32-S3-Zero**. Der Router hängt an einem freien CRSF-Port des Empfängers und verteilt CRSF auf zwei weitere Ports (Port B und Port C). So lassen sich mehrere Module an einem Empfänger betreiben und verketten: RC-Multiswitch C3/S3, Multiswitch Basis (Ziege-One), Soundmodule, G030-Multiswitch und weitere Router.

> **Stand:** Firmware v0.3.0 als fertiger Arduino-Sketch auf Basis von **RC_Common 1.2.0** (`firmware/RC_CRSF_Router/`). Gebaut über GitHub Actions, **noch nicht auf Hardware getestet**. Das erste Gerüst v0.2.0 liegt zum Vergleich in `firmware/v0.2.0-geruest/`.

## Funktionen (v0.3.0)

- Weiterleitung Empfänger → Port B/C und zurück, Ketten über mehrere Stufen (RC_Common-Router). Nach unten geht alles außer Anfragen an die eigene Adresse, nach oben alles, zwischen B und C nie.
- Jeder Port wählbar: Aus / CRSF / S.Port / GPS / Raw (Lua und WebUI), gilt nach Neustart
- Eigene WM-Moduladresse 0–15 (Vorgabe 15 = 0xCF), mehrere Router im Modell möglich, Warnung bei doppelter Adresse
- Status-LED in Farben (siehe unten)
- Telemetrie: Empfängerspannung (als RxBt oder Zellen-Wert), Temperatur des ESP32, FLVSS über S.Port, GPS
- Lua-Menü am Sender: Ports, Adresse, Telemetrie, Zähler, WLAN, Neustart
- WebUI: Status, Ports, Telemetrie, WLAN, Backups im Modul, Export/Import, Firmware-Update (OTA)

Geplant für später: Geräteliste und Telemetrie-Übersicht über den ganzen Baum (RC_Common 1.3.0), Telemetrie-Bremse, Parameter-Browser in der WebUI.

## Status-LED

| LED | Bedeutung |
|---|---|
| grün langsam | RC-Signal in Ordnung |
| violett langsam | RC-Signal in Ordnung, WLAN an |
| rot schnell | kein RC-Signal / Failsafe |
| gelb Doppelblitz | Adresskonflikt oder doppelte Port-Rolle |
| blau blinkend (3 s nach dem Start) | jetzt BOOT drücken = WLAN an |

## WLAN

Access Point **CRSF-Router**, Passwort `123456789`, Seite http://192.168.1.1. Einschalten über BOOT-Taste (3 s nach dem Start), Lua „WLAN jetzt“, automatisch nach 60 s ohne RC-Signal oder „WLAN dauerhaft an“ (Vorgabe aus).

## Arduino IDE

Board **ESP32S3 Dev Module**, USB CDC On Boot: Enabled, Flash Size 4MB, Partition Scheme „Default 4MB with spiffs“, Core **esp32 3.0.7**, Bibliothek **TinyGPSPlus 1.0.3**. Sketch: `firmware/RC_CRSF_Router/RC_CRSF_Router.ino`. Fertige Images liegen bei jedem Build unter GitHub Actions (`*.ino.bin` für OTA, `*.merged.bin` für USB ab 0x0).

## Pins (S3-Zero, wie Sound Mini S3)

| Funktion | Pins |
|---|---|
| Uplink zum Empfänger (CRSF) | RX 5, TX 6 |
| Port B | RX 7, TX 8 |
| Port C | RX 44, TX 43 |
| Status-LED | GPIO21 |
| Empfängerspannung | GPIO1 über Teiler 33 kΩ / 10 kΩ |

Alle Ports vollduplex. S.Port wird extern mit einer Diode (1N4148) auf eine Leitung gebracht.

## Aufbau des Repos

| Ordner | Inhalt |
|---|---|
| `firmware/` | Firmware (Gerüst v0.2.0, später Arduino-Sketch) |
| `hardware/` | Schaltplan und Platine (folgt) |
| `docs/` | Projektbeschreibung (`PROJEKT.md`), später Abnahmeprüfung |

## Verwandte Projekte

- [RC-Multiswitch-ESP32-S3](https://github.com/mnorden81/RC-Multiswitch-ESP32-S3) / [-C3](https://github.com/mnorden81/RC-Multiswitch-ESP32-C3)
- [ESP32-Sound-Mini-S3](https://github.com/mnorden81/ESP32-Sound-Mini-S3)
- [ESP32-Multiswitch-Basis_Ziege-One](https://github.com/mnorden81/ESP32-Multiswitch-Basis_Ziege-One)
- [rcmultiswitchG030_eigen](https://github.com/mnorden81/rcmultiswitchG030_eigen)
