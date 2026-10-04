#pragma once

// Hardware-independent rules shared by the device and the PC tests.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace thermo {
constexpr const char *VERSION = "wifi-discord-1.1.1";
constexpr size_t HISTORY_CAPACITY = 120;
constexpr uint32_t HISTORY_STEP_SECONDS = 720;
constexpr size_t HISTORY_BYTES = 12 + HISTORY_CAPACITY * 16 + 4;
constexpr uint8_t TEMP_HIGH = 1, HUM_HIGH = 2, SENSOR_ERROR = 4;

struct Settings {
  char name[64] = "家の温湿度計";
  uint32_t sampleSeconds = 30;
  uint32_t reportMinutes = 30;
  uint32_t saveMinutes = 12;
  uint32_t holdSeconds = 120;
  uint32_t repeatMinutes = 30;
  float tempHigh = 30.0f;
  float tempLow = 0.0f;
  float humHigh = 80.0f;
  float tempHysteresis = 1.0f;
  float humHysteresis = 5.0f;
  bool tempEnabled = true;
  bool humEnabled = true;
  bool buzzerEnabled = true;
  bool screenAlwaysOn = true;
};

inline bool validMeasurement(float t, float h) {
  return std::isfinite(t) && std::isfinite(h) && t >= -40 && t <= 85 && h >= 0 && h <= 100;
}
inline bool validEpoch(uint32_t epoch) { return epoch >= 1704067200U && epoch < 4102444800U; }
inline uint8_t sensorCrc(const uint8_t *bytes, size_t length) {
  uint8_t crc = 0xff;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (int b = 0; b < 8; ++b) crc = uint8_t((crc << 1) ^ ((crc & 0x80) ? 0x31 : 0));
  }
  return crc;
}
inline bool decodeSensor(const uint8_t *bytes, size_t length, float &t, float &h) {
  if (length != 7 || (bytes[0] & 0x80) || !(bytes[0] & 0x08) || sensorCrc(bytes, 6) != bytes[6]) return false;
  uint32_t rawH = uint32_t(bytes[1]) << 12 | uint32_t(bytes[2]) << 4 | (bytes[3] >> 4);
  uint32_t rawT = uint32_t(bytes[3] & 0x0f) << 16 | uint32_t(bytes[4]) << 8 | bytes[5];
  float temperature = float(rawT) * (200.0f / 1048576.0f) - 50.0f;
  float humidity = float(rawH) * (100.0f / 1048576.0f);
  if (!validMeasurement(temperature, humidity)) return false;
  t = temperature; h = humidity; return true;
}
inline bool validSettings(const Settings &s) {
  return s.name[0] && std::memchr(s.name, 0, sizeof(s.name)) &&
         s.sampleSeconds >= 10 && s.sampleSeconds <= 300 && s.reportMinutes <= 1440 &&
         s.saveMinutes >= 1 && s.saveMinutes <= 120 && s.holdSeconds >= 10 && s.holdSeconds <= 3600 &&
         s.repeatMinutes >= 1 && s.repeatMinutes <= 1440 &&
         validMeasurement(s.tempHigh, s.humHigh) && std::isfinite(s.tempLow) && s.tempLow >= -40 && s.tempLow < s.tempHigh &&
         std::isfinite(s.tempHysteresis) && s.tempHysteresis > 0 && s.tempHysteresis <= 10 &&
         std::isfinite(s.humHysteresis) && s.humHysteresis > 0 && s.humHysteresis <= 20;
}

// Only the canonical Discord host/path is allowed; no redirects, queries or extra paths.
inline bool validWebhook(const char *url) {
  const char *p = nullptr;
  const char *prefixes[] = {"https://discord.com/api/webhooks/", "https://discord.com/api/v10/webhooks/"};
  for (const char *prefix : prefixes)
    if (std::strncmp(url, prefix, std::strlen(prefix)) == 0) p = url + std::strlen(prefix);
  if (!p) return false;
  size_t digits = 0;
  while (*p >= '0' && *p <= '9') { ++p; ++digits; }
  if (digits < 16 || digits > 21 || *p++ != '/') return false;
  size_t token = 0;
  for (; *p; ++p, ++token)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
          (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.')) return false;
  return token >= 30 && token <= 150;
}

struct Gate {
  bool active = false, pending = false, target = false;
  uint64_t since = 0;
  void observe(bool wanted, bool valid, uint64_t now, uint64_t hold) {
    if (!valid || wanted == active) { pending = false; return; }
    if (!pending || target != wanted) { pending = true; target = wanted; since = now; }
    if (now - since >= hold) { active = target; pending = false; }
  }
};

struct Alarms {
  Gate temp, hum, sensor;
  uint8_t mask() const { return (temp.active ? TEMP_HIGH : 0) | (hum.active ? HUM_HIGH : 0) |
                                 (sensor.active ? SENSOR_ERROR : 0); }
  uint8_t observe(float t, float h, bool valid, uint64_t now, const Settings &s) {
    uint64_t hold = uint64_t(s.holdSeconds) * 1000;
    sensor.observe(!valid, true, now, hold);
    if (!s.tempEnabled) temp = Gate{};
    else temp.observe(temp.active ? t > s.tempHigh - s.tempHysteresis : t >= s.tempHigh,
                      valid, now, hold);
    if (!s.humEnabled) hum = Gate{};
    else hum.observe(hum.active ? h > s.humHigh - s.humHysteresis : h >= s.humHigh,
                     valid, now, hold);
    return mask();
  }
};

enum class Reason : uint8_t { None, Startup, Alarm, Recovery, Changed, Reconnected, Reminder, Periodic, Test };
enum class Outcome : uint8_t { Success, Transient, RateLimited, Permanent };
inline Outcome classifyHttp(int code) {
  if (code == 200) return Outcome::Success; // wait=true confirms the saved message.
  if (code == 429) return Outcome::RateLimited;
  if (code <= 0 || code == 408 || code == 425 || code >= 500) return Outcome::Transient;
  return Outcome::Permanent;
}
inline uint64_t secondsToMs(double seconds) {
  if (!std::isfinite(seconds) || seconds <= 0) return 0;
  // Values larger than a year are represented by a year, never a short retry loop.
  if (seconds > 31536000.0) seconds = 31536000.0;
  return static_cast<uint64_t>(std::ceil(seconds * 1000.0));
}

struct Notifications {
  bool delivered = false, busy = false, blocked = false, wasOffline = false;
  uint8_t deliveredMask = 0, inFlightMask = 0, failures = 0;
  uint64_t lastSuccess = 0, nextAttempt = 0;
  void reconfigure() {
    uint64_t wait = nextAttempt;
    *this = Notifications{}; nextAttempt = wait;
  }
  Reason candidate(uint64_t now, uint8_t mask, bool ready, const Settings &s) {
    if (!ready) { wasOffline = true; return Reason::None; }
    if (blocked || busy || now < nextAttempt) return Reason::None;
    if (!delivered) return Reason::Startup;
    if (mask != deliveredMask) {
      if (!mask) return Reason::Recovery;
      if (!deliveredMask) return Reason::Alarm;
      return Reason::Changed;
    }
    if (wasOffline && now - lastSuccess >= 60000) return Reason::Reconnected;
    if (mask && now - lastSuccess >= uint64_t(s.repeatMinutes) * 60000) return Reason::Reminder;
    if (s.reportMinutes && now - lastSuccess >= uint64_t(s.reportMinutes) * 60000) return Reason::Periodic;
    return Reason::None;
  }
  void begin(uint8_t mask) { busy = true; inFlightMask = mask; }
  void finish(Outcome result, uint64_t now, uint64_t serverWaitMs = 0) {
    busy = false;
    if (result == Outcome::Success) {
      delivered = true; deliveredMask = inFlightMask; lastSuccess = now;
      wasOffline = false; failures = 0;
      nextAttempt = now + (serverWaitMs > 5000 ? serverWaitMs : 5000);
    } else if (result == Outcome::Permanent) {
      blocked = true; // Only explicit reconfiguration/test resumes an invalid webhook.
    } else {
      ++failures;
      uint64_t backoff = uint64_t(15000) << (failures - 1);
      if (failures >= 3) { backoff = 15 * 60000; failures = 0; }
      nextAttempt = now + (serverWaitMs > backoff ? serverWaitMs : backoff);
    }
  }
};

struct Point { uint32_t epoch = 0; float temperature = 0, humidity = 0; bool startsSegment = false; };
inline uint32_t crc32(const uint8_t *bytes, size_t length) {
  uint32_t crc = 0xffffffff;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}
inline void put32(uint8_t *p, uint32_t n) { for (int i = 0; i < 4; ++i) p[i] = uint8_t(n >> (8 * i)); }
inline uint32_t get32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline bool newerGeneration(uint32_t a, uint32_t b) { return int32_t(a - b) > 0; }

struct History {
  Point points[HISTORY_CAPACITY]{};
  size_t count = 0;
  bool append(const Point &p) {
    if (!validEpoch(p.epoch) || !validMeasurement(p.temperature, p.humidity) ||
        (count && p.epoch <= points[count - 1].epoch)) return false;
    if (count == HISTORY_CAPACITY) {
      std::memmove(points, points + 1, sizeof(Point) * (HISTORY_CAPACITY - 1)); --count;
    }
    points[count++] = p;
    return true;
  }
  bool pruneFuture(uint32_t now) {
    size_t old = count;
    while (count && points[count - 1].epoch > now) --count;
    return count != old;
  }
  void encode(uint8_t *out, uint32_t generation) const {
    static_assert(sizeof(float) == 4, "32-bit float required");
    std::memset(out, 0, HISTORY_BYTES);
    put32(out, 0x31485754U); // TWH1; explicit little-endian, no struct padding.
    out[4] = 1; out[6] = uint8_t(count); put32(out + 8, generation);
    for (size_t i = 0; i < count; ++i) {
      uint8_t *p = out + 12 + i * 16;
      uint32_t t, h;
      std::memcpy(&t, &points[i].temperature, 4); std::memcpy(&h, &points[i].humidity, 4);
      put32(p, points[i].epoch); put32(p + 4, t); put32(p + 8, h);
      put32(p + 12, points[i].startsSegment ? 1 : 0);
    }
    put32(out + HISTORY_BYTES - 4, crc32(out, HISTORY_BYTES - 4));
  }
  bool decode(const uint8_t *in, size_t length, uint32_t &generation) {
    if (length != HISTORY_BYTES || get32(in) != 0x31485754U || in[4] != 1 || in[5] || in[7] ||
        in[6] > HISTORY_CAPACITY || get32(in + HISTORY_BYTES - 4) != crc32(in, HISTORY_BYTES - 4)) return false;
    History candidate;
    for (size_t i = 0; i < in[6]; ++i) {
      const uint8_t *p = in + 12 + i * 16;
      Point point; point.epoch = get32(p);
      uint32_t t = get32(p + 4), h = get32(p + 8);
      std::memcpy(&point.temperature, &t, 4); std::memcpy(&point.humidity, &h, 4);
      uint32_t flags = get32(p + 12);
      if (flags > 1) return false;
      point.startsSegment = flags != 0;
      if (!candidate.append(point)) return false;
    }
    *this = candidate; generation = get32(in + 8); return true;
  }
};

inline bool connectedPoints(const Point &a, const Point &b) {
  return !b.startsSegment && b.epoch > a.epoch && b.epoch - a.epoch <= HISTORY_STEP_SECONDS + 300;
}
inline int graphX(uint32_t timestamp, uint32_t now, uint32_t rangeSeconds, int left, int right) {
  if (timestamp >= now) return right;
  uint32_t age = now - timestamp;
  if (age >= rangeSeconds) return left;
  return right - int(uint64_t(age) * uint32_t(right - left) / rangeSeconds);
}
} // namespace thermo
