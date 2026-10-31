#pragma once
#include <Arduino.h>
#include <Preferences.h>

enum PortMode : uint8_t { MODE_OFF = 0, MODE_CRSF = 1, MODE_SPORT = 2 };

struct Config {
  uint8_t modeA = MODE_CRSF;
  uint8_t modeB = MODE_CRSF;
  uint8_t addrA = 0xC0;   // CRSF-Adresse des Geraets an Port A
  uint8_t addrB = 0xC1;   // CRSF-Adresse des Geraets an Port B
};

inline void loadConfig(Config &c) {
  Preferences p;
  p.begin("router", true);
  c.modeA = p.getUChar("modeA", c.modeA);
  c.modeB = p.getUChar("modeB", c.modeB);
  c.addrA = p.getUChar("addrA", c.addrA);
  c.addrB = p.getUChar("addrB", c.addrB);
  p.end();
  if (c.modeA > MODE_SPORT) c.modeA = MODE_CRSF;
  if (c.modeB > MODE_SPORT) c.modeB = MODE_CRSF;
}

inline void saveConfig(const Config &c) {
  Preferences p;
  p.begin("router", false);
  p.putUChar("modeA", c.modeA);
  p.putUChar("modeB", c.modeB);
  p.putUChar("addrA", c.addrA);
  p.putUChar("addrB", c.addrB);
  p.end();
}
