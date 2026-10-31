// CRSF-Router v0.2.0 - ESP32-S3
// - Adress-Routing zwischen Uplink (Empfaenger) und Port A/B
// - Parameter-Menue (Lua): Modus je Port (Aus / CRSF / S.Port), CRSF-Adresse je Port, Speichern & Neustart
// - Einstellungen in NVS
// Noch offen: WebUI, S.Port-Treiber (Port-Modus S.Port oeffnet den Port derzeit nur), Halbduplex an Port A/B
#include <Arduino.h>
#include "crsf.h"
#include "config.h"

#define FW_VERSION_STR_(a, b, c) #a "." #b "." #c
#define FW_VERSION_STR(a, b, c) FW_VERSION_STR_(a, b, c)

// ---- Pins (anpassen!) ----
constexpr int PIN_UP_RX = 44, PIN_UP_TX = 43;   // Uplink: UART0, vollduplex zum Empfaenger
constexpr int PIN_A_RX  = 4,  PIN_A_TX  = 5;    // Port A: UART1
constexpr int PIN_B_RX  = 6,  PIN_B_TX  = 7;    // Port B: UART2

constexpr uint32_t CRSF_BAUD  = 420000;
constexpr uint32_t SPORT_BAUD = 57600;
constexpr uint8_t  ADDR_ROUTER = 0xCA;          // Platzhalter

// ---- CRSF-Parametertypen ----
constexpr uint8_t PT_UINT8   = 0;
constexpr uint8_t PT_TEXTSEL = 9;
constexpr uint8_t PT_COMMAND = 13;
constexpr uint8_t PT_HIDDEN  = 0x80;

enum ParamId : uint8_t { P_MODE_A = 1, P_MODE_B, P_ADDR_A, P_ADDR_B, P_SAVE, P_COUNT = P_SAVE };

HardwareSerial &uplink = Serial0;
HardwareSerial portA(1);
HardwareSerial portB(2);

CrsfParser pUp, pA, pB;
FrameQueue<16> qUp;      // Frames Richtung Empfaenger

Config cfg;              // aktive Konfiguration (beim Boot geladen)
Config edit;             // im Menue bearbeitet, wirkt nach Speichern + Neustart
uint32_t restartAt = 0;
bool saving = false;

static bool crsfA() { return cfg.modeA == MODE_CRSF; }
static bool crsfB() { return cfg.modeB == MODE_CRSF; }
static void sendFrame(HardwareSerial &s, const Frame &f) { s.write(f.buf, f.size); }

// ---------- DEVICE_INFO ----------
static void sendDeviceInfo(uint8_t dest) {
  uint8_t d[40];
  uint8_t n = 0;
  const char *name = "CRSF Router";
  memcpy(d + n, name, strlen(name) + 1); n += strlen(name) + 1;
  memset(d + n, 0, 4); n += 4;                       // Seriennummer
  memset(d + n, 0, 4); n += 4;                       // HW-Version
  d[n++] = 0; d[n++] = FW_VERSION_MAJOR; d[n++] = FW_VERSION_MINOR; d[n++] = FW_VERSION_PATCH;
  d[n++] = P_COUNT;                                  // Anzahl Parameter
  d[n++] = 0;                                        // Parameter-Protokollversion
  Frame f;
  buildExt(f, T_DEVICE_INFO, dest, ADDR_ROUTER, d, n);
  qUp.push(f);
}

// ---------- Parameter ----------
static void addStr(uint8_t *d, uint8_t &n, const char *s) { size_t l = strlen(s) + 1; memcpy(d + n, s, l); n += l; }

static void sendParamEntry(uint8_t dest, uint8_t id) {
  uint8_t d[CRSF_MAX_FRAME];
  uint8_t n = 0;
  d[n++] = id;
  d[n++] = 0;      // Chunks remaining
  d[n++] = 0;      // Parent (Root)

  switch (id) {
    case P_MODE_A:
    case P_MODE_B: {
      uint8_t v = (id == P_MODE_A) ? edit.modeA : edit.modeB;
      d[n++] = PT_TEXTSEL;
      addStr(d, n, id == P_MODE_A ? "Port A Modus" : "Port B Modus");
      addStr(d, n, "Aus;CRSF;S.Port");
      d[n++] = v; d[n++] = 0; d[n++] = 2; d[n++] = MODE_CRSF;
      addStr(d, n, "");
      break;
    }
    case P_ADDR_A:
    case P_ADDR_B: {
      bool a = (id == P_ADDR_A);
      uint8_t mode = a ? edit.modeA : edit.modeB;
      d[n++] = PT_UINT8 | (mode == MODE_CRSF ? 0 : PT_HIDDEN);
      addStr(d, n, a ? "Port A Adresse" : "Port B Adresse");
      d[n++] = a ? edit.addrA : edit.addrB; d[n++] = 0; d[n++] = 255; d[n++] = a ? 0xC0 : 0xC1;
      addStr(d, n, "");
      break;
    }
    case P_SAVE:
      d[n++] = PT_COMMAND;
      addStr(d, n, "Speichern + Neustart");
      d[n++] = saving ? 2 : 0;    // Status: 0 bereit, 2 laeuft
      d[n++] = 20;                // Timeout (x100 ms)
      addStr(d, n, saving ? "Speichere..." : "");
      break;
    default:
      return;
  }
  Frame f;
  buildExt(f, T_PARAM_ENTRY, dest, ADDR_ROUTER, d, n);
  qUp.push(f);
}

static void handleParamWrite(const Frame &f) {
  if (f.buf[1] < 6) return;
  uint8_t id = f.buf[5], v = f.buf[6], src = f.src();
  switch (id) {
    case P_MODE_A: if (v <= MODE_SPORT) edit.modeA = v; break;
    case P_MODE_B: if (v <= MODE_SPORT) edit.modeB = v; break;
    case P_ADDR_A: edit.addrA = v; break;
    case P_ADDR_B: edit.addrB = v; break;
    case P_SAVE:
      if (v == 1 && !saving) {
        saveConfig(edit);
        saving = true;
        restartAt = millis() + 400;
        sendParamEntry(src, P_SAVE);
      }
      break;
  }
}

static void handleParamRead(const Frame &f) {
  if (f.buf[1] < 6) return;
  uint8_t id = f.buf[5];
  if (id >= 1 && id <= P_COUNT) sendParamEntry(f.src(), id);
}

// ---------- Routing ----------
static void routeFromUplink(const Frame &f) {
  uint8_t t = f.type();
  if (!isExtended(t)) {                              // RC-Channels, Link-Stats usw.
    if (crsfA()) sendFrame(portA, f);
    if (crsfB()) sendFrame(portB, f);
    return;
  }
  uint8_t dst = f.dest();
  if (dst == ADDR_ROUTER || dst == ADDR_BROADCAST) {
    if (t == T_DEVICE_PING)      sendDeviceInfo(f.src());
    else if (t == T_PARAM_READ)  handleParamRead(f);
    else if (t == T_PARAM_WRITE) handleParamWrite(f);
  }
  if (dst == ADDR_BROADCAST) {
    if (crsfA()) sendFrame(portA, f);
    if (crsfB()) sendFrame(portB, f);
  } else if (dst == cfg.addrA && crsfA()) {
    sendFrame(portA, f);
  } else if (dst == cfg.addrB && crsfB()) {
    sendFrame(portB, f);
  }
}

static void routeFromPort(const Frame &f) { qUp.push(f); }   // unveraendert Richtung Empfaenger

static void pump(HardwareSerial &s, CrsfParser &p, void (*cb)(const Frame &)) {
  Frame f;
  while (s.available()) {
    if (p.feed((uint8_t)s.read(), f)) cb(f);
  }
}

static void beginPort(HardwareSerial &s, uint8_t mode, int rx, int tx) {
  if (mode == MODE_CRSF)       s.begin(CRSF_BAUD, SERIAL_8N1, rx, tx);
  else if (mode == MODE_SPORT) s.begin(SPORT_BAUD, SERIAL_8N1, rx, tx, true);   // invertiert; Treiber folgt
}

void setup() {
  Serial.begin(115200);  // USB-CDC Debug
  loadConfig(cfg);
  edit = cfg;
  uplink.begin(CRSF_BAUD, SERIAL_8N1, PIN_UP_RX, PIN_UP_TX);
  beginPort(portA, cfg.modeA, PIN_A_RX, PIN_A_TX);
  beginPort(portB, cfg.modeB, PIN_B_RX, PIN_B_TX);
  Serial.printf("CRSF-Router v%s | A=%u/0x%02X B=%u/0x%02X\n",
                FW_VERSION_STR(FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH),
                cfg.modeA, cfg.addrA, cfg.modeB, cfg.addrB);
}

void loop() {
  pump(uplink, pUp, routeFromUplink);
  if (crsfA()) pump(portA, pA, routeFromPort);
  if (crsfB()) pump(portB, pB, routeFromPort);

  Frame f;
  while (qUp.pop(f)) sendFrame(uplink, f);

  if (restartAt && (int32_t)(millis() - restartAt) >= 0) ESP.restart();
}
