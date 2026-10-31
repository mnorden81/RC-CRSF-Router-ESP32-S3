# Firmware

| Ordner | Stand |
|---|---|
| `RC_CRSF_Router/` | **v0.3.0**, Arduino-Sketch auf RC_Common 1.2.0. Gebaut per GitHub Actions, nicht auf Hardware getestet. |
| `v0.2.0-geruest/` | Erstes Gerüst von Martin (PlatformIO, eigener CRSF-Code). Nicht kompiliert, nicht getestet. Bleibt als Ausgangsstand. |

`RC_CRSF_Router/src/RC_Common/` ist eine unveränderte Kopie von RC_Common (gleich wie in RC-Multiswitch und Soundmodulen). Änderungen daran immer in allen Kopien nachziehen, die CI prüft das mit `MANIFEST.sha256`.
