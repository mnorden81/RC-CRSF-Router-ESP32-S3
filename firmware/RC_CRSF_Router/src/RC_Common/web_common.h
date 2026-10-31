// web_common.h - RC_Common: gemeinsamer Kern der Weboberflaechen
//
// Teile, die jede Weboberflaeche gleich hat:
//   POST /api/otaupdate  Firmware-Update per WLAN (.bin, multipart/form-data)
//   GET  /rc_common.js   gemeinsames JavaScript: OTA-Upload mit Fortschritt,
//                        Rollen- und Baudraten-Listen der Zusatz-UARTs,
//                        Fehlertexte, Auswahllisten fuellen
//   GET  /api/uartroles  Rollen und Baudraten als JSON
// dazu kleine JSON-Helfer fuer die REST-APIs der Projekte.
// Die projektspezifischen Seiten und APIs bleiben im Projekt.
// Die Flash-Backups (/api/backup/*) registriert config_backup.h, ihr
// JavaScript (rcBackup*) liegt mit in /rc_common.js.
#ifndef RC_WEB_COMMON_H
#define RC_WEB_COMMON_H

#ifdef ARDUINO
#include <Arduino.h>
#include <WebServer.h>

// Registriert /api/otaupdate, /rc_common.js und /api/uartroles.
// beforeRestart (optional) laeuft nach erfolgreichem Update vor dem
// Neustart, z. B. um offene Einstellungen zu speichern.
void webCommonRegister(WebServer& server, void (*beforeRestart)() = nullptr);

String webJsonEscape(const char* s);
bool   webJsonInt(const String& body, const char* key, int& out);
bool   webJsonBool(const String& body, const char* key, bool& out);
bool   webJsonString(const String& body, const char* key, String& out);
#endif

#endif
