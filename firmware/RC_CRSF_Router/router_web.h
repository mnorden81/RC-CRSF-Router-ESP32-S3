#ifndef ROUTER_WEB_H
#define ROUTER_WEB_H

/*
 * router_web.h  -  CRSF-Router: WLAN-Access-Point und Weboberflaeche
 *
 *   GET  /              Weboberflaeche (webui.h)
 *   GET  /api/status    Zustand als JSON (liefert die .ino: routerStatusJson())
 *   GET  /api/config    Einstellungen als JSON (auch Export/Backup)
 *   POST /api/config    Einstellungen setzen (auch Import/Backup laden)
 *   POST /api/restart   Neustart
 * dazu aus RC_Common: /api/otaupdate, /rc_common.js, /api/uartroles,
 * /api/backup/* (Einstellungs-Backups im Flash).
 */

#include <Arduino.h>

void   webBegin();           // Routen registrieren (AP bleibt aus)
void   webEnableAP();
void   webDisableAP();
bool   webApActive();
void   webLoop();            // handleClient, nur wenn der AP laeuft

// von der .ino geliefert
String routerStatusJson();
void   routerRequestRestart();

#endif
