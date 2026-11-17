# Versionsgeschichte RC-CRSF-Router-ESP32-S3

Die Version wird bei jeder Änderung erhöht, auch die CRSF „Parameter Version Number“ im DEVICE_INFO. Neueste Version oben.

## v0.3.8 (RC_Common 1.4.0)
- Status-LED einheitlich mit allen Modulen: kein Signal = rot Aufblitzen (vorher rot schnell), rot schnell = Frames, aber Failsafe / kein RC-Link, WLAN an = violett auch beim Aufblitzen. Farben und Muster aus RC_Common.

## v0.3.7 (RC_Common 1.3.0)
- Lua übersichtlicher: Ordner „Ports“ zeigt die Rollen, Baudrate bei Port „Aus“ und „Spannung Korrektur“ bei „Spannung senden: Aus“ ausgeblendet (Feld-IDs bleiben).

## v0.3.6
- Adresskonflikt deutlicher: LED blau Doppelblitz (gelb nur noch bei doppelter Port-Rolle), Lua, WebUI und USB zeigen Port B oder C. CRSF_ESP32 1.1.1.

## v0.3.5
- Port A auf RX44/TX43 (Serial0), Port C auf Serial2 (7/8). GPIO14/15 liegen beim S3-Zero nicht auf der Stiftleiste. Auf Hardware geprüft (04.11.2026).

## v0.3.4
- Pins: Port A RX14/TX15, Port B RX5/TX6, Port C RX7/TX8 (der Fehler bei v0.3.1 war ein Wackler in der Verkabelung).
- RC_Common 1.2.1: lange Lua-Felder (Port-Baudraten) kommen Chunk für Chunk, vorher fehlte der Ordner „Ports“.

## v0.3.3
- Pins wieder wie v0.3.0 (A 5/6, B 7/8, C 44/43).
- Fehlersuche Eingang: Bytes, Frames und CRC-Fehler von Port A auf USB (alle 2 s) und in der WebUI.
- BOOT jederzeit 2 s halten = WLAN an/aus. Ohne Signal und mit WLAN blinkt die LED violett.

## v0.3.2
- LED-Farben korrigiert (S3-Zero: Rot/Grün vertauscht), Lua „LED Reihenfolge“.

## v0.3.1
- Neue Pins: Port A RX14/TX15, Port B RX5/TX6, Port C RX7/TX8.

## v0.3.0
- Erste Firmware auf RC_Common 1.2.0: Router, Port-Rollen, Lua, WebUI, farbige Status-LED, Spannung und Temperatur als Telemetrie.

## v0.2.0
- Gerüst mit eigenem CRSF-Code (PlatformIO), ungetestet. Liegt als Ausgangsstand in `firmware/v0.2.0-geruest/`.
