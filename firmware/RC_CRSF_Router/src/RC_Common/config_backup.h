// config_backup.h - RC_Common: Einstellungs-Backups im Flash (LittleFS)
//
// Speichert die Konfiguration als JSON-Datei im Flash des Moduls, in der
// Partition "spiffs" (Partitionsschema "Default 4MB with spiffs"). Der
// Inhalt ist derselbe wie beim Export als Datei im Browser: die Weboberflaeche
// sammelt die Konfiguration und schickt sie an /api/backup/save, beim
// Wiederherstellen holt sie die Datei und spielt sie mit ihrem normalen
// Import ein. Das Modul selbst kennt den Inhalt nicht.
//
//   GET  /api/backup/list            {"ok":true,"files":[{name,size,auto}],"free":n}
//   POST /api/backup/save[?auto=1&when=YYYYMMDD-HHMM]   Body = JSON
//   GET  /api/backup/get?file=NAME   die gespeicherte Datei
//   POST /api/backup/delete?file=NAME
//
// Dateien: /backup/cfg-NNNN-YYYYMMDD-HHMM[-auto].json, NNNN fortlaufend.
// Gehalten werden die letzten CFG_BACKUP_KEEP von Hand gesicherten und die
// letzten CFG_BACKUP_KEEP_AUTO automatischen (vor Import und Firmware-Update).
// Das Dateisystem wird erst beim ersten Zugriff eingebunden (eine leere
// Partition wird dabei einmalig formatiert), damit der Start nicht wartet.
//
// Das Backup uebersteht Flashen aus der IDE und OTA-Updates, nicht aber
// "Erase All Flash", einen Wechsel des Partitionsschemas oder ein
// Hochladen des Dateisystem-Abbilds (pio run -t uploadfs).
#ifndef RC_CONFIG_BACKUP_H
#define RC_CONFIG_BACKUP_H

#include <stdint.h>
#include <stddef.h>

#define CFG_BACKUP_KEEP       5
#define CFG_BACKUP_KEEP_AUTO  3
#define CFG_BACKUP_MAX_SIZE   32768
#define CFG_BACKUP_NAME_LEN   40

// Prueft einen Dateinamen streng auf das eigene Muster (verhindert Pfade
// wie ../ in ?file=). Liefert die laufende Nummer, sonst -1. isAuto (optional)
// sagt, ob es ein automatisches Backup ist.
int  cfgBackupParseName(const char* name, bool* isAuto = nullptr);

// Prueft "YYYYMMDD-HHMM" (nur Ziffern an den richtigen Stellen).
bool cfgBackupWhenValid(const char* when);

// Baut den Dateinamen ohne Pfad. when darf leer/ungueltig sein, dann
// "00000000-0000".
void cfgBackupMakeName(char* out, size_t n, int number, const char* when, bool isAuto);

// Index der Datei, die als naechstes weg muss: die mit der kleinsten Nummer
// der Art isAuto, aber nur wenn es von dieser Art mehr als keep gibt.
// Sonst -1. Fremde Namen zaehlen nicht mit.
int  cfgBackupPruneIndex(const char* const* names, int count, bool isAuto, int keep);

#ifdef ARDUINO
#include <WebServer.h>

// Registriert /api/backup/*. Ohne diesen Aufruf hat das Projekt keine
// Flash-Backups (z. B. Soundmodul Basis, das auf die SD-Karte sichert).
void cfgBackupRegister(WebServer& server);
bool cfgBackupMount();   // true = Dateisystem bereit
#endif

#endif
