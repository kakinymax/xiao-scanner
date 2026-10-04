#pragma once
#include "thermo_core.h"

namespace thermo {
constexpr uint8_t TEMP_LOW = 8;
constexpr uint32_t DAY_SECONDS = 86400, JST_SECONDS = 9 * 3600;
inline uint32_t jstDay(uint32_t epoch) { return (epoch + JST_SECONDS) / DAY_SECONDS; }
inline uint32_t dayStart(uint32_t day) { return day * DAY_SECONDS - JST_SECONDS; }

// Boundaries are calendar days in JST, never a moving 24-hour window on retry.
struct DailySchedule {
  uint32_t observedDay = 0, targetDay = 0, deliveredDay = 0;
  History frozen;
  bool advance(uint32_t epoch, const History &history) {
    if (!validEpoch(epoch)) return false;
    uint32_t day = jstDay(epoch);
    if (!observedDay) { observedDay = day; return true; }
    if (day <= observedDay) return false; // Ignore backward clock corrections.
    observedDay = day;
    uint32_t target = day - 1;
    if (target <= deliveredDay || target <= targetDay) return true;
    targetDay = target; frozen.count = 0;
    uint32_t begin = dayStart(target), end = dayStart(target + 1);
    for (size_t i = 0; i < history.count; ++i)
      if (history.points[i].epoch >= begin && history.points[i].epoch < end)
        frozen.append(history.points[i]);
    return true;
  }
  bool complete(uint32_t day) {
    if (!day || day > targetDay) return false;
    if (day > deliveredDay) deliveredDay = day;
    if (day == targetDay) { targetDay = 0; frozen.count = 0; }
    return true;
  }
};

enum class AlertEvent : uint8_t { None, Abnormal, Recovery, Changed };
inline const char *alertTitle(AlertEvent event, uint8_t mask) {
  if (event == AlertEvent::Recovery) return "正常な状態への復帰";
  if (event == AlertEvent::Changed) return "異常状態の変化";
  if ((mask & SENSOR_ERROR) && !(mask & (TEMP_HIGH | TEMP_LOW))) return "センサーの取得異常";
  return "温度条件の超過・悪化";
}
inline const char *readingState(uint8_t mask, bool valid) {
  return mask ? "条件: " : valid ? "状態: 通常" : "状態: センサー確認中";
}
// Only hourly samples advance the highest/lowest notified temperature band.
// Cooling while still abnormal never lowers that boundary.
struct HourlyMonitor {
  uint32_t lastHour = 0, revision = 0, measuredEpoch = 0;
  uint8_t mask = 0, pendingMask = 0;
  float nextHigh = 0, nextLow = 0, temperature = 0, humidity = 0;
  AlertEvent pending = AlertEvent::None;
  bool observe(uint32_t epoch, float t, float h, bool valid, bool enabled, float high, float low) {
    if (!validEpoch(epoch) || epoch / 3600 <= lastHour) return false;
    lastHour = epoch / 3600;
    uint8_t before = mask;
    bool worsening = false;
    if (!valid || !validMeasurement(t, h)) mask |= SENSOR_ERROR;
    else {
      mask = enabled ? (t > high ? TEMP_HIGH : t < low ? TEMP_LOW : 0) : 0;
      if (mask & TEMP_HIGH) {
        worsening = !(before & TEMP_HIGH) || t >= nextHigh;
        if (worsening) nextHigh = std::floor(t) + 1;
      } else if (mask & TEMP_LOW) {
        worsening = !(before & TEMP_LOW) || t <= nextLow;
        if (worsening) nextLow = std::ceil(t) - 1;
      } else { nextHigh = 0; nextLow = 0; }
    }
    AlertEvent event = AlertEvent::None;
    if (mask != before) event = !mask ? AlertEvent::Recovery : !before ? AlertEvent::Abnormal : AlertEvent::Changed;
    else if (worsening) event = AlertEvent::Abnormal;
    if (event != AlertEvent::None) {
      pending = event; pendingMask = mask; temperature = valid ? t : 0; humidity = valid ? h : 0;
      measuredEpoch = valid ? epoch : 0; ++revision;
    }
    return true;
  }
  void complete(uint32_t sentRevision) { if (sentRevision == revision) pending = AlertEvent::None; }
};

// Explicit format plus CRC, written alternately to two NVS slots. No credentials.
constexpr size_t DAILY_STATE_BYTES = 68 + HISTORY_BYTES + 4;
struct DailyState {
  DailySchedule daily;
  HourlyMonitor monitor;
  void encode(uint8_t *out, uint32_t generation) const {
    std::memset(out, 0, DAILY_STATE_BYTES);
    put32(out, 0x31445954U); put32(out + 4, generation); // TYD1
    put32(out + 8, daily.observedDay); put32(out + 12, daily.targetDay); put32(out + 16, daily.deliveredDay);
    put32(out + 20, monitor.lastHour); put32(out + 24, monitor.revision); put32(out + 28, monitor.measuredEpoch);
    out[32] = monitor.mask; out[33] = monitor.pendingMask; out[34] = uint8_t(monitor.pending);
    const float values[] = {monitor.nextHigh, monitor.nextLow, monitor.temperature, monitor.humidity};
    for (size_t i = 0; i < 4; ++i) { uint32_t bits; std::memcpy(&bits, values + i, 4); put32(out + 36 + i * 4, bits); }
    daily.frozen.encode(out + 68, generation);
    put32(out + DAILY_STATE_BYTES - 4, crc32(out, DAILY_STATE_BYTES - 4));
  }
  bool decode(const uint8_t *in, size_t length, uint32_t &generation) {
    if (length != DAILY_STATE_BYTES || get32(in) != 0x31445954U ||
        get32(in + length - 4) != crc32(in, length - 4)) return false;
    DailyState c;
    c.daily.observedDay = get32(in + 8); c.daily.targetDay = get32(in + 12); c.daily.deliveredDay = get32(in + 16);
    c.monitor.lastHour = get32(in + 20); c.monitor.revision = get32(in + 24); c.monitor.measuredEpoch = get32(in + 28);
    c.monitor.mask = in[32]; c.monitor.pendingMask = in[33]; c.monitor.pending = AlertEvent(in[34]);
    if ((in[32] | in[33]) & ~(TEMP_HIGH | TEMP_LOW | SENSOR_ERROR) || in[34] > uint8_t(AlertEvent::Changed) || in[35]) return false;
    float *values[] = {&c.monitor.nextHigh, &c.monitor.nextLow, &c.monitor.temperature, &c.monitor.humidity};
    for (size_t i = 0; i < 4; ++i) {
      uint32_t bits = get32(in + 36 + i * 4); std::memcpy(values[i], &bits, 4);
      if (!std::isfinite(*values[i])) return false;
    }
    for (size_t i = 52; i < 68; ++i) if (in[i]) return false;
    if (c.daily.observedDay && !validEpoch(dayStart(c.daily.observedDay))) return false;
    if (c.daily.targetDay && (!c.daily.observedDay || c.daily.targetDay >= c.daily.observedDay || c.daily.targetDay <= c.daily.deliveredDay)) return false;
    uint32_t historyGeneration;
    if (!c.daily.frozen.decode(in + 68, HISTORY_BYTES, historyGeneration) || historyGeneration != get32(in + 4)) return false;
    *this = c; generation = get32(in + 4); return true;
  }
};
} // namespace thermo
