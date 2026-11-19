# Prüfstand: Module per Funk verbinden (Funk-Brücke)

Autor: PiperPilot · Version 0.1.9 · noch nicht auf Hardware getestet

Hier testen wir, ob die Module statt per Kabel auch per Funk (ESP-NOW) zuverlässig CRSF bekommen. Die bestehende Firmware der Module bleibt dabei **unverändert**.

## Idee

Zwei kleine ESP32 („Funk-Brücken“) ersetzen das CRSF-Kabel:

```
ELRS-Empfänger --Kabel--> Brücke 1  ~~ ESP-NOW ~~>  Brücke 2 --Kabel--> Sound Mini S3
                                                                       (oder jedes andere Modul)
```

- Jedes gültige CRSF-Frame vom Kabel wird gefunkt, jedes gefunkte Frame aufs Kabel geschrieben.
- Damit laufen **Kanäle, Telemetrie und Lua-Menü** über Funk.
- Beide Brücken bekommen **dieselbe Firmware**, es gibt nichts umzuschalten.
- Mehrere Brücken mit gleicher Fahrzeug-Kennung bilden ein Netz.
- **Failsafe:** Fällt der Funk aus, schreibt Brücke 2 nichts mehr aufs Kabel. Das Modul merkt „kein CRSF“ und geht in seinen normalen Failsafe.

## Was du brauchst

- 2 freie ESP32 (S3-Zero, ESP32-C3 oder klassischer ESP32, auch gemischt)
- ELRS-Empfänger und Sender
- ein Modul zum Testen, z. B. Sound Mini S3

## Anschluss

| Board | CRSF RX | CRSF TX | LED |
|---|---|---|---|
| ESP32-S3-Zero | GPIO5 | GPIO6 | GPIO21 (RGB) |
| ESP32-C3 | GPIO20 | GPIO21 | GPIO8 (unbestätigt) |
| ESP32 (klassisch) | GPIO16 | GPIO17 | GPIO2 |

- **Brücke 1:** RX an TX des Empfängers, TX an RX des Empfängers, GND an GND, 5 V an 5 V.
- **Brücke 2:** RX an TX des Moduls (CRSF-Port), TX an RX des Moduls, GND, 5 V.
- RX und TX also immer **über Kreuz**.

## Flashen

Die fertigen Dateien baut GitHub automatisch: Reiter **Actions** → „Prüfstand Funk-Brücke bauen“ → neuester Lauf → unten bei **Artifacts** `funk-bruecke-s3`, `-c3` oder `-esp32` herunterladen.

- `Funk_Bruecke_xx_komplett.bin` per USB ab Adresse **0x0** flashen (wie in der Flash-Anleitung im Haupt-README).
- Oder in der Arduino IDE öffnen: `pruefstand/Funk_Bruecke/Funk_Bruecke.ino`, Board „ESP32S3 Dev Module“ bzw. „ESP32C3 Dev Module“, USB CDC On Boot **an**.

## Bedienung

- WLAN „Funk-Bruecke-XXXX“, Passwort `123456789`, Seite **http://192.168.1.1**
- Dort stehen alle Messwerte, jede Sekunde neu, und die Einstellungen.
- **BOOT 2 s halten** = WLAN an/aus.
- Gleiche Messwerte auch im seriellen Monitor (115200).

**LED**

| LED | Bedeutung |
|---|---|
| grün blinkt langsam | Funk-Verbindung da, Kanäle kommen |
| grün blinkt schnell | Funk-Verbindung da, aber keine Kanäle |
| rot blinkt | keine Funk-Verbindung |
| kurz blau alle 2 s | WLAN ist an |

Sind Rot und Grün vertauscht (beim Start grün, mit Verbindung rot), in der WebUI „LED-Reihenfolge“ umstellen. Standard ist GRB.

**Einstellungen** (bei beiden Brücken gleich einstellen!)

- Fahrzeug-Kennung 1–255 (Standard 1)
- Funkkanal 1–13 (Standard 1)
- Sendeleistung 2–20 dBm (Standard 8)
- CRSF-Baudrate (Standard 420000)
- „Kanäle höchstens alle … ms funken“ (Standard 0 = jedes Frame)

## Testablauf

1. **Grundtest:** Beide Brücken an, Empfänger gebunden. Brücke 2 zeigt „Funk-Verbindung da“, Kanal-Frames aus Funk ungefähr so viele wie die ELRS-Paketrate.
2. **Modul:** Sound Mini an Brücke 2. Gas geben → Motorsound reagiert ohne spürbare Verzögerung.
3. **Lua-Menü:** Am Sender „ExpressLRS“/Geräte öffnen → Sound Mini erscheint und lässt sich einstellen.
4. **Failsafe:** Brücke 1 vom Strom trennen → Brücke 2 rot, Sound Mini geht in Failsafe. Wieder einschalten → läuft sofort wieder.
5. **Messwerte notieren** (Seite von Brücke 2): verloren in %, Aussetzer > 200 ms, längste Lücke, Laufzeit (eine Richtung).
6. **ELRS-Störung:** Test 5 mit Sendeleistung 2, 8 und 20 dBm wiederholen und am Sender auf LQ/RSSI schauen. Brücken einmal direkt neben der Empfängerantenne, einmal 20 cm entfernt.
7. **ELRS-Paketrate:** einmal mit 150 Hz und einmal mit 500 Hz testen.
8. Optional: WLAN aus (BOOT 2 s) und Messwerte vergleichen.

**Gutes Ergebnis:** verloren unter 1 %, keine Aussetzer > 200 ms, Laufzeit unter 3 ms, ELRS-LQ bleibt gleich.

## Grenzen dieses Prüfstands

- Die Laufzeit misst nur den Funkweg, nicht das Kabel davor und danach.
- Bei 500 Hz belegt der Funk viel Sendezeit. Mit „Kanäle höchstens alle 4 ms“ lässt sich das begrenzen.
- Keine Verschlüsselung, nur die Fahrzeug-Kennung trennt Fahrzeuge.
