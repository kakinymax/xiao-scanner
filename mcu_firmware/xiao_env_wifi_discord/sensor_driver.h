#pragma once
#include "thermo_core.h"
#include <Wire.h>

// AHT20 measurement timing follows the ASAIR datasheet; no unbounded BUSY loop.
class SensorDriver {
  enum class Phase { Idle, Initialize, Measure };
  Phase phase = Phase::Idle;
  uint64_t started = 0, nextRead = 0;
  bool command(uint8_t command, uint8_t a, uint8_t b, bool parameters = true) {
    Wire.beginTransmission(0x38); Wire.write(command);
    if (parameters) { Wire.write(a); Wire.write(b); }
    return Wire.endTransmission() == 0;
  }
  int status() {
    if (!command(0x71, 0, 0, false) || Wire.requestFrom(uint8_t(0x38), uint8_t(1)) != 1) return -1;
    return Wire.read();
  }
  bool trigger(uint64_t now) {
    if (!command(0xac, 0x33, 0)) return false;
    phase = Phase::Measure; started = now; nextRead = now + 80; return true;
  }
public:
  bool active() const { return phase != Phase::Idle; }
  bool start(uint64_t now) {
    int s = status();
    if (s < 0 || (s & 0x80)) return false;
    if (!(s & 0x08)) {
      if (!command(0xbe, 0x08, 0)) return false;
      phase = Phase::Initialize; started = now; nextRead = now + 10; return true;
    }
    return trigger(now);
  }
  bool poll(uint64_t now, float &t, float &h, bool &valid) {
    if (!active() || now < nextRead) return false;
    valid = false;
    if (now - started >= 500) { phase = Phase::Idle; return true; }
    if (phase == Phase::Initialize) {
      int s = status();
      if (s >= 0 && !(s & 0x80) && (s & 0x08) && trigger(now)) return false;
      phase = Phase::Idle; return true;
    }
    uint8_t bytes[7];
    if (Wire.requestFrom(uint8_t(0x38), uint8_t(7)) != 7) { phase = Phase::Idle; return true; }
    for (uint8_t &byte : bytes) byte = uint8_t(Wire.read());
    if (bytes[0] & 0x80) { nextRead = now + 10; return false; }
    valid = thermo::decodeSensor(bytes, sizeof(bytes), t, h);
    phase = Phase::Idle; return true;
  }
};
