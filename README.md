# RC-CRSF-Router-ESP32-S3

CRSF-Verteiler für ELRS-Empfänger auf einem **ESP32-S3-Zero**. Der Router hängt an einem freien CRSF-Port des Empfängers und verteilt CRSF auf zwei weitere Ports (Port B und Port C). So lassen sich mehrere Module an einem Empfänger betreiben und verketten: RC-Multiswitch C3/S3, Multiswitch Basis (Ziege-One), Soundmodule, G030-Multiswitch und weitere Router.

> **Stand:** Planung. Im Ordner `firmware/v0.2.0-geruest/` liegt das erste Gerüst (PlatformIO, nicht kompiliert, nicht getestet). Die nächste Firmware entsteht als fertiger Arduino-Sketch auf Basis von **RC_Common**.

## Geplante Funktionen

- Weiterleitung Empfänger → Port B/C und zurück, Ketten über mehrere Stufen (RC_Common-Router)
- Jeder Port wählbar: Aus / CRSF / S.Port / GPS (Lua und WebUI)
- Eigene WM-Moduladresse 0–15 (Vorgabe 15 = 0xCF), mehrere Router im Modell möglich
- Status-LED (WS2812, GPIO21)
- Telemetrie: Empfängerspannung, Temperatur, FLVSS über S.Port, GPS
- Geräteliste und Telemetrie-Übersicht über den ganzen Baum
- WebUI mit OTA, Backups, WLAN wie bei allen Modulen

## Geplante Pins (S3-Zero, wie Sound Mini S3)

| Funktion | Pins |
|---|---|
| Uplink zum Empfänger (CRSF) | RX 5, TX 6 |
| Port B | RX 7, TX 8 |
| Port C | RX 44, TX 43 |
| Status-LED | GPIO21 |

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
