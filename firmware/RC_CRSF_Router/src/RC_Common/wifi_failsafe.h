// wifi_failsafe.h - RC_Common: WLAN-Auto-Failsafe
//
// Schaltet den Access Point ein, wenn laenger als die eingestellte Zeit kein
// gueltiges RC-Signal anliegt (z. B. Modell auf der Werkbank, Sender aus).
// Schaltet NIE automatisch wieder aus, sonst bricht eine laufende
// Konfiguration oder ein OTA-Update ab, sobald kurz wieder Signal kommt.
// Mit "WLAN dauerhaft an" laeuft der AP schon seit dem Booten.
//
// Reine Logik ohne Arduino-Abhaengigkeit (Host-Tests in tests/).
#ifndef RC_WIFI_FAILSAFE_H
#define RC_WIFI_FAILSAFE_H

#include <stdint.h>

class WifiFailsafe {
public:
    // Jeden Loop aufrufen. true = AP jetzt einschalten.
    bool check(unsigned long nowMs, bool alwaysOn, bool autoOn, uint16_t timeoutSec,
               bool busOk, bool apActive) {
        if (alwaysOn || busOk || !autoOn || apActive) { pending = false; return false; }
        if (!pending) { pending = true; sinceMs = nowMs; return false; }
        return nowMs - sinceMs >= (unsigned long)timeoutSec * 1000UL;
    }
private:
    unsigned long sinceMs = 0;
    bool          pending = false;
};

#endif
