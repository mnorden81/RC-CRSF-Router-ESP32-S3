# Firmware

| Ordner | Stand |
|---|---|
| `RC_CRSF_Router/` | Arduino-Sketch auf RC_Common 1.4.0, aktuelle Version siehe [CHANGELOG.md](../CHANGELOG.md). Gebaut per GitHub Actions. |
| `v0.2.0-geruest/` | Erstes Gerüst von PiperPilot (PlatformIO, eigener CRSF-Code). Nicht kompiliert, nicht getestet. Bleibt als Ausgangsstand. |

`RC_CRSF_Router/src/RC_Common/` ist eine unveränderte Kopie von RC_Common (gleich wie in RC-Multiswitch und Soundmodulen). Änderungen daran immer in allen Kopien nachziehen, die CI prüft das mit `MANIFEST.sha256`.
