# Steckplan: Funk-Brücke mit zwei ESP32-S3-Zero auf dem Breadboard

Autor: PiperPilot · Firmware v0.1.9

## Was du brauchst

- 2 × ESP32-S3-Zero mit eingelöteten Stiftleisten
- 1 Breadboard (ein großes, oder zwei kleine)
- ELRS-Empfänger mit 4-poligem Kabel (5V, GND, TX, RX)
- 6–8 Steckbrücken
- 2 USB-C-**Daten**kabel

Du brauchst nur die Leiste mit **5V, GND, 3V3, GP1 … GP6**. Darauf liegen alle benötigten Pins: 5V, GND, GPIO5 und GPIO6.

## Zero auf das Breadboard stecken

- Jeden Zero so stecken, dass er **über die Mittelrille** reicht. Dann liegen die beiden Stiftleisten in getrennten Reihen.
- Zero 1 links, Zero 2 rechts auf dem Breadboard, mit etwas Abstand (ca. 10 Reihen). Bei zwei kleinen Breadboards bekommt jeder Zero sein eigenes.
- USB-Buchse nach außen, damit beide Kabel gut passen.
- Mit einem Stift „1“ und „2“ auf das Breadboard schreiben.

```
        Zero 1 (am Empfänger)                    Zero 2 (allein / an Modul)
   ┌──────────────────────┐                 ┌──────────────────────┐
   │  USB   ESP32-S3-Zero │                 │  USB   ESP32-S3-Zero │
   │ 5V GND 3V3 1 2 3 4 5 6│                │ 5V GND 3V3 1 2 3 4 5 6│
   └─┬──┬──────────────┬─┬┘                 └────┬──────────────┬─┬┘
     │  │              │ │                       │              │ │
     │  │              │ └─ TX ─► RX Empfänger   │ (GND frei)   │ └─ (TX frei)
     │  │              └─── RX ◄─ TX Empfänger   │              └─── (RX frei)
     │  └── GND ────────────── GND Empfänger
     └───── 5V ─────────────── 5V Empfänger
```

## Zero 1 an den ELRS-Empfänger (4 Kabel)

| Zero 1 | Kabelfarbe (Vorschlag) | ELRS-Empfänger |
|---|---|---|
| **5V** | rot | **5V** |
| **GND** | schwarz | **GND** |
| **GPIO5** (RX) | gelb | **TX** |
| **GPIO6** (TX) | weiß | **RX** |

- TX und RX **über Kreuz**: GPIO5 an TX vom Empfänger.
- Zero 1 bekommt Strom über **USB**, und der Empfänger bekommt seinen Strom über den 5V-Pin von Zero 1.

## Zero 2 (Gegenstelle)

**Für den ersten Test nichts anschließen**, nur USB. Ab v0.1.6 hält die Firmware den freien RX-Pin selbst ruhig.

Später ein CRSF-Modul dazu, zum Beispiel einen Multiswitch, an der Stelle, wo sonst das Empfängerkabel steckt:

| Zero 2 | Modul (CRSF-Eingang) |
|---|---|
| **GND** | **GND** |
| **GPIO5** (RX) | **TX** des Moduls |
| **GPIO6** (TX) | **RX** des Moduls |

Das Modul bekommt seinen eigenen Strom. GND muss trotzdem verbunden sein.

## Flashen

1. Arduino IDE: **Datei → Öffnen** → `Funk_Bruecke.ino` (im gleichen Ordner wie diese Datei).
2. Werkzeuge: Board **ESP32S3 Dev Module**, USB CDC On Boot **Enabled**, Flash Size **4MB**, Partition **Default 4MB with spiffs**.
3. Zero 1 anstecken, Port wählen, **Hochladen**. Dann das Gleiche mit Zero 2.
4. Klappt das Hochladen nicht: BOOT gedrückt halten beim Einstecken.

## Inbetriebnahme

1. Erst **Zero 1** per USB einschalten, dann **Zero 2**.
2. Sender einschalten, und der Empfänger verbindet sich.
3. Erwartet:
   - **Zero 1**: grün **langsam** blinkend (Funk + Kanäle vom Kabel)
   - **Zero 2**: grün **langsam** blinkend (Funk + Kanäle aus dem Funk)
   - Beide alle 2 s kurz **blau** (WLAN an)
4. WLANs: jeder Zero hat jetzt ein **eigenes** WLAN „Funk-Bruecke-XXXX“. Die XXXX sind die letzten 4 Zeichen der MAC. Passwort `123456789`, Seite http://192.168.1.1.
5. Auf der Seite von **Zero 1**: „Kanal-Frames vom Kabel /s“ ist größer als 0.
6. Auf der Seite von **Zero 2**: „Kanal-Frames aus Funk /s“ ist etwa genauso groß wie bei Zero 1.

## Wenn es nicht klappt

| Anzeige | Bedeutung / Abhilfe |
|---|---|
| Zero 1 grün **schnell** | Funk ok, aber vom Empfänger kommt nichts: GPIO5 an **TX** prüfen, GND prüfen, Empfänger-LED muss dauerhaft leuchten |
| beide **rot** | keine Funk-Verbindung: Kennung und Kanal auf beiden Seiten gleich? |
| Farben vertauscht | WebUI → „LED-Reihenfolge“ umstellen |
| LED leuchtet **dauerhaft** | Firmware hängt: RESET drücken, Version prüfen (v0.1.9) |
