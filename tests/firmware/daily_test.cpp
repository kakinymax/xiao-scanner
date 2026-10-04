#include "device_config.h"
#include "daily_core.h"
#include "trend_png.h"
#include <cstdio>
#include <cstdlib>

static int assertions = 0;
#define CHECK(v) do { ++assertions; if (!(v)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #v); std::exit(1); } } while (0)
using namespace thermo;
constexpr uint32_t OCT4 = 1791039600U;

static void temperatureBands() {
  CHECK(!std::strcmp(readingState(0, false), "状態: センサー確認中"));
  CHECK(!std::strcmp(readingState(0, true), "状態: 通常"));
  CHECK(!std::strcmp(alertTitle(AlertEvent::Abnormal, SENSOR_ERROR), "センサーの取得異常"));
  HourlyMonitor m;
  uint32_t hour = OCT4;
  auto observe = [&](float t) { bool changed = m.observe(hour, t, 60, true, true, 40, 0); hour += 3600; return changed; };
  CHECK(observe(40)); CHECK(m.mask == 0 && m.pending == AlertEvent::None);
  CHECK(observe(40.5f)); CHECK(m.mask == TEMP_HIGH && m.nextHigh == 41 && m.pending == AlertEvent::Abnormal);
  uint32_t first = m.revision;
  m.complete(first); CHECK(m.pending == AlertEvent::None);
  CHECK(observe(40.5f)); CHECK(m.revision == first && m.pending == AlertEvent::None);
  CHECK(observe(40.5f)); CHECK(m.revision == first);
  CHECK(observe(39)); CHECK(m.pending == AlertEvent::Recovery && m.mask == 0);
  m.complete(m.revision);
  CHECK(observe(40.5f)); CHECK(m.nextHigh == 41 && m.pending == AlertEvent::Abnormal);
  m.complete(m.revision);
  CHECK(observe(41)); CHECK(m.nextHigh == 42 && m.pending == AlertEvent::Abnormal);
  m.complete(m.revision); first = m.revision;
  CHECK(observe(40.1f)); CHECK(m.nextHigh == 42 && m.pending == AlertEvent::None);
  CHECK(observe(41.9f)); CHECK(m.revision == first && m.pending == AlertEvent::None);
  CHECK(observe(42.8f)); CHECK(m.nextHigh == 43);
  m.complete(m.revision);
  CHECK(observe(40)); CHECK(m.pending == AlertEvent::Recovery && m.nextHigh == 0);
  m.complete(m.revision);
  CHECK(observe(0)); CHECK(m.pending == AlertEvent::None);
  CHECK(observe(-0.5f)); CHECK(m.mask == TEMP_LOW && m.nextLow == -1);
  m.complete(m.revision); first = m.revision;
  CHECK(observe(-0.9f)); CHECK(m.revision == first);
  CHECK(observe(-1)); CHECK(m.nextLow == -2 && m.pending == AlertEvent::Abnormal);
  m.complete(m.revision);
  CHECK(observe(-0.5f)); CHECK(m.nextLow == -2 && m.pending == AlertEvent::None);
  CHECK(observe(0)); CHECK(m.mask == 0 && m.pending == AlertEvent::Recovery);
  CHECK(!m.observe(hour - 3600 + 300, -10, 60, true, true, 40, 0)); // no intra-hour repeat
  m.complete(m.revision);
  CHECK(m.observe(hour, 70, 60, false, true, 40, 0)); CHECK(m.mask == SENSOR_ERROR);
  hour += 3600; m.complete(m.revision); first = m.revision;
  CHECK(m.observe(hour, 0, 0, false, true, 40, 0)); CHECK(m.revision == first && m.pending == AlertEvent::None);
  hour += 3600;
  CHECK(m.observe(hour, 25, 60, true, true, 40, 0)); CHECK(m.pending == AlertEvent::Recovery);
  m.complete(first); CHECK(m.pending == AlertEvent::Recovery); // old in-flight result cannot clear a newer event
  std::puts("hourly high/low bands, monotonic worsening boundaries, exact recovery, missing data: PASS");
}

static void calendarAndPersistence() {
  History h;
  CHECK(h.append({OCT4 - 1, 20, 50}));
  CHECK(h.append({OCT4, 25, 60})); CHECK(h.append({OCT4 + 3600, 26, 61, true}));
  CHECK(h.append({OCT4 + 86400, 27, 62}));
  DailyState s;
  CHECK(s.daily.advance(OCT4 + 100, h)); CHECK(!s.daily.targetDay); // no retroactive startup report
  CHECK(!s.daily.advance(OCT4 + 86399, h));
  CHECK(s.daily.advance(OCT4 + 86400, h));
  CHECK(s.daily.targetDay == jstDay(OCT4) && s.daily.frozen.count == 2);
  CHECK(s.daily.frozen.points[0].epoch == OCT4 && s.daily.frozen.points[1].startsSegment);
  h = History{}; CHECK(s.daily.frozen.count == 2); // retry is independent of rolling history
  uint32_t target = s.daily.targetDay;
  CHECK(!s.daily.advance(OCT4 + 86000, h)); // NTP backward correction
  CHECK(s.monitor.observe(OCT4 + 86400, 40.5f, 60, true, true, 40, 0));
  uint8_t bytes[DAILY_STATE_BYTES]; s.encode(bytes, UINT32_MAX);
  DailyState reboot; uint32_t generation = 0;
  CHECK(reboot.decode(bytes, sizeof(bytes), generation)); CHECK(generation == UINT32_MAX);
  CHECK(reboot.daily.targetDay == target && reboot.daily.frozen.count == 2 && reboot.monitor.nextHigh == 41);
  CHECK(!reboot.monitor.observe(OCT4 + 86400 + 100, 41, 60, true, true, 40, 0));
  CHECK(!reboot.daily.advance(OCT4 + 86400 + 100, h));
  CHECK(reboot.daily.complete(target)); CHECK(!reboot.daily.targetDay && reboot.daily.deliveredDay == target);
  reboot.encode(bytes, 0); CHECK(s.decode(bytes, sizeof(bytes), generation) && generation == 0);
  CHECK(!s.daily.advance(OCT4 + 86400 + 900, h) && !s.daily.targetDay);
  CHECK(s.daily.advance(OCT4 + 4 * 86400, h)); CHECK(s.daily.targetDay == target + 3); // coalesce missed days
  uint32_t latest = s.daily.targetDay;
  CHECK(s.daily.complete(target + 1)); CHECK(s.daily.targetDay == latest); // older success doesn't clear latest
  CHECK(!s.daily.complete(latest + 1));
  s.encode(bytes, 1); bytes[100] ^= 1;
  CHECK(!reboot.decode(bytes, sizeof(bytes), generation)); CHECK(reboot.daily.deliveredDay == target);
  CHECK(!reboot.decode(nullptr, 0, generation));
  DailySchedule year;
  CHECK(year.advance(1798729200U - 1, h)); CHECK(year.advance(1798729200U, h));
  CHECK(dayStart(year.targetDay + 1) == 1798729200U); // December to January in JST, UTC is previous date
  Configuration c;
  const char *url = "https://discord.com/api/webhooks/123456789012345678/PLACEHOLDER_NOT_A_REAL_TOKEN_123456";
  cJSON *node = cJSON_CreateObject(); cJSON_AddStringToObject(node, "alert_webhook_url", url);
  CHECK(configurationFromJson(node, c)); cJSON_Delete(node);
  node = configurationToJson(c, false); char *publicJson = cJSON_PrintUnformatted(node);
  CHECK(!std::strstr(publicJson, "PLACEHOLDER") && !std::strstr(publicJson, "alert_webhook_url"));
  CHECK(std::strstr(publicJson, "temp_low")); cJSON_free(publicJson); cJSON_Delete(node);
  node = cJSON_Parse("{\"temp_high\":0,\"temp_low\":40}"); CHECK(!configurationFromJson(node, c)); cJSON_Delete(node);
  CHECK(c.settings.tempHigh == 40 && c.settings.tempLow == 0);
  std::puts("JST midnight/year boundary, frozen retries, reboot deduplication, dual-slot format/CRC, two private routes: PASS");
}

static void pngFixtures() {
  History h;
  for (uint32_t i = 0; i < 120; ++i) {
    if (i >= 35 && i < 60) continue; // visible gap, no synthetic values
    CHECK(h.append({OCT4 + i * 720, 25 + 8 * std::sin(float(i) / 20), 65 - 15 * std::sin(float(i) / 20), i == 60}));
  }
  static uint8_t out[TREND_BODY_BYTES + 2]; std::memset(out, 0x5a, sizeof(out));
  size_t n = renderTrendPng(out + 1, TREND_PNG_BYTES, h, OCT4, OCT4 + 86400,
    "2026/10/04 00:00 - 2026/10/05 00:00 JST");
  CHECK(n == TREND_PNG_BYTES && out[0] == 0x5a && out[n + 1] == 0x5a);
  FILE *file = std::fopen("/output/trend-sample.png", "wb"); CHECK(file);
  CHECK(std::fwrite(out + 1, 1, n, file) == n); CHECK(std::fclose(file) == 0);
  CHECK(!renderTrendPng(out, TREND_PNG_BYTES - 1, h, OCT4, OCT4 + 86400, "short"));
  CHECK(!renderTrendPng(out, TREND_PNG_BYTES, h, OCT4, OCT4, "invalid"));
  History empty;
  n = renderTrendPng(out, sizeof(out), empty, OCT4, OCT4 + 86400, "2026/10/04 00:00 - 2026/10/05 00:00 JST");
  file = std::fopen("/output/trend-empty.png", "wb"); CHECK(file); CHECK(std::fwrite(out, 1, n, file) == n); std::fclose(file);
  n = trendMultipart(out, TREND_BODY_BYTES, "{\"content\":\"test\",\"allowed_mentions\":{\"parse\":[]}}", h, OCT4, OCT4 + 86400, "2026/10/04 00:00 - 2026/10/05 00:00 JST");
  CHECK(n > TREND_PNG_BYTES && n < TREND_BODY_BYTES);
  file = std::fopen("/output/trend-multipart.bin", "wb"); CHECK(file); CHECK(std::fwrite(out, 1, n, file) == n); std::fclose(file);
  std::puts("bounded device PNG/multipart fixtures exported for independent decoding and visual inspection: PASS");
}
int main() {
  temperatureBands(); calendarAndPersistence(); pngFixtures();
  std::printf("PASS: %d assertions, 3 daily-notification scenario groups\n", assertions);
}
