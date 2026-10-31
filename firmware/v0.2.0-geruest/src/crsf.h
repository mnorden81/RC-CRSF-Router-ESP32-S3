#pragma once
#include <Arduino.h>

// ---- CRSF Konstanten ----
constexpr uint8_t CRSF_SYNC            = 0xC8;
constexpr uint8_t CRSF_MAX_FRAME       = 64;

constexpr uint8_t T_RC_CHANNELS        = 0x16;
constexpr uint8_t T_DEVICE_PING        = 0x28;
constexpr uint8_t T_DEVICE_INFO        = 0x29;
constexpr uint8_t T_PARAM_ENTRY        = 0x2B;
constexpr uint8_t T_PARAM_READ         = 0x2C;
constexpr uint8_t T_PARAM_WRITE        = 0x2D;

constexpr uint8_t ADDR_BROADCAST       = 0x00;

inline bool isExtended(uint8_t type) { return type >= 0x28 && type <= 0x96; }

// ---- CRC8 (Poly 0xD5) ----
inline uint8_t crc8(const uint8_t *d, uint8_t len) {
  uint8_t crc = 0;
  while (len--) {
    crc ^= *d++;
    for (uint8_t i = 0; i < 8; i++) crc = (crc & 0x80) ? (crc << 1) ^ 0xD5 : (crc << 1);
  }
  return crc;
}

// ---- Frame: buf[0]=sync/addr, buf[1]=len, buf[2]=type, ..., buf[len+1]=crc ----
struct Frame {
  uint8_t buf[CRSF_MAX_FRAME];
  uint8_t size = 0;  // Gesamtlaenge in Bytes

  uint8_t type() const { return buf[2]; }
  // Extended: buf[3]=dest, buf[4]=src
  uint8_t dest() const { return buf[3]; }
  uint8_t src() const { return buf[4]; }
  uint8_t payloadLen() const { return buf[1] - 2; }          // ohne type + crc
  const uint8_t *payload() const { return buf + 3; }

  void finalize() { buf[buf[1] + 1] = crc8(buf + 2, buf[1] - 1); size = buf[1] + 2; }
};

// Baut Extended-Frame: dest, src, daten
inline void buildExt(Frame &f, uint8_t type, uint8_t dest, uint8_t src, const uint8_t *data, uint8_t n) {
  f.buf[0] = CRSF_SYNC;
  f.buf[1] = 1 + 2 + n + 1;  // type + dest + src + n + crc
  f.buf[2] = type;
  f.buf[3] = dest;
  f.buf[4] = src;
  if (n) memcpy(f.buf + 5, data, n);
  f.finalize();
}

// ---- Byte-Parser ----
class CrsfParser {
 public:
  // true, wenn Frame komplett und CRC ok
  bool feed(uint8_t b, Frame &out) {
    if (pos_ == 0) {
      // Sync: gueltige Adressen akzeptieren (breit, Filter ueber Laenge/CRC)
      if (b == 0xC8 || b == 0xEA || b == 0xEC || b == 0xEE || b == 0x00) { buf_[pos_++] = b; }
      return false;
    }
    buf_[pos_++] = b;
    if (pos_ == 2) {
      if (b < 2 || b > CRSF_MAX_FRAME - 2) { pos_ = 0; return false; }
      need_ = b + 2;
    }
    if (pos_ >= 2 && pos_ == need_) {
      pos_ = 0;
      if (crc8(buf_ + 2, need_ - 3) == buf_[need_ - 1]) {
        memcpy(out.buf, buf_, need_);
        out.size = need_;
        return true;
      }
    }
    return false;
  }
 private:
  uint8_t buf_[CRSF_MAX_FRAME];
  uint8_t pos_ = 0, need_ = 0;
};

// ---- einfache Frame-Queue ----
template <uint8_t N>
class FrameQueue {
 public:
  bool push(const Frame &f) { uint8_t n = (h_ + 1) % N; if (n == t_) return false; q_[h_] = f; h_ = n; return true; }
  bool pop(Frame &f) { if (h_ == t_) return false; f = q_[t_]; t_ = (t_ + 1) % N; return true; }
 private:
  Frame q_[N]; uint8_t h_ = 0, t_ = 0;
};
