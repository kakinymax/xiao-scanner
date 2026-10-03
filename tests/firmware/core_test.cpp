#include "device_config.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

static int assertions = 0;
#define CHECK(value) do { ++assertions; if (!(value)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #value); std::exit(1); } } while (0)

static void sensorAndSettings() {
  using namespace thermo;
  Settings settings;
  CHECK(validSettings(settings));
  settings.sampleSeconds = 0; CHECK(!validSettings(settings));
  settings = Settings{}; settings.reportMinutes = 0; CHECK(validSettings(settings));
  settings.humHysteresis = 0; CHECK(!validSettings(settings));
  settings = Settings{}; std::memset(settings.name, 'x', sizeof(settings.name)); CHECK(!validSettings(settings));
  CHECK(!validMeasurement(std::numeric_limits<float>::quiet_NaN(), 50));
  CHECK(!validMeasurement(20, 101));
  CHECK(validMeasurement(-40, 100));
  const uint8_t reference[] = {0x08, 0x80, 0x00, 0x06, 0x00, 0x00, 0xa6};
  float t = -99, h = -99;
  CHECK(decodeSensor(reference, sizeof(reference), t, h));
  CHECK(t == 25 && h == 50);
  CHECK(!decodeSensor(reference, 6, t, h));
  uint8_t packet[7]; std::memcpy(packet, reference, 7); packet[6] ^= 1;
  CHECK(!decodeSensor(packet, 7, t, h));
  std::memcpy(packet, reference, 7); packet[0] |= 0x80; packet[6] = sensorCrc(packet, 6);
  CHECK(!decodeSensor(packet, 7, t, h));
  packet[0] = 0; packet[6] = sensorCrc(packet, 6); CHECK(!decodeSensor(packet, 7, t, h));
  std::puts("sensor conversion, CRC, invalid/busy/uncalibrated frames, settings bounds: PASS");
}

static void alarmBoundaries() {
  using namespace thermo;
  Settings s; Alarms a;
  CHECK(a.observe(30, 79, true, 0, s) == 0);
  CHECK(a.observe(30, 79, true, 119999, s) == 0);
  CHECK(a.observe(30, 79, true, 120000, s) == TEMP_HIGH);
  CHECK(a.observe(29.5f, 79, true, 130000, s) == TEMP_HIGH);
  CHECK(a.observe(29, 79, true, 140000, s) == TEMP_HIGH);
  CHECK(a.observe(29, 79, true, 260000, s) == 0);
  CHECK(a.observe(31, 80, true, 300000, s) == 0);
  CHECK(a.observe(31, 80, true, 420000, s) == (TEMP_HIGH | HUM_HIGH));
  CHECK(a.observe(31, 75, true, 450000, s) == (TEMP_HIGH | HUM_HIGH));
  CHECK(a.observe(31, 75, true, 570000, s) == TEMP_HIGH);
  // Missing readings cannot be mistaken for recovery and reset pending transitions.
  CHECK(a.observe(0, 0, false, 600000, s) == TEMP_HIGH);
  CHECK(a.observe(0, 0, false, 720000, s) == (TEMP_HIGH | SENSOR_ERROR));
  CHECK(a.observe(28, 50, true, 730000, s) == (TEMP_HIGH | SENSOR_ERROR));
  CHECK(a.observe(28, 50, true, 850000, s) == 0);
  Alarms flicker;
  flicker.observe(30, 50, true, 0, s); flicker.observe(29.9f, 50, true, 100000, s);
  CHECK(flicker.observe(30, 50, true, 120000, s) == 0);
  CHECK(flicker.observe(30, 50, true, 239999, s) == 0);
  CHECK(flicker.observe(30, 50, true, 240000, s) == TEMP_HIGH);
  s.tempEnabled = false; CHECK(flicker.observe(40, 50, true, 250000, s) == 0);
  // Uptime exceeds the 32-bit millis() wrap at 49.7 days.
  Alarms longRun; uint64_t start = uint64_t(UINT32_MAX) + 100;
  longRun.observe(30, 50, true, start, Settings{});
  CHECK(longRun.observe(30, 50, true, start + 120000, Settings{}) == TEMP_HIGH);
  std::puts("alarm hold, exact thresholds, hysteresis, partial recovery, missing readings, long uptime: PASS");
}

static void notificationLifecycle() {
  using namespace thermo;
  Settings s; Notifications n;
  CHECK(n.candidate(0, 0, false, s) == Reason::None);
  CHECK(n.candidate(1000, 0, true, s) == Reason::Startup);
  n.begin(0); CHECK(n.candidate(2000, TEMP_HIGH, true, s) == Reason::None);
  n.finish(Outcome::Success, 2000);
  CHECK(n.candidate(6999, TEMP_HIGH, true, s) == Reason::None);
  CHECK(n.candidate(7000, TEMP_HIGH, true, s) == Reason::Alarm);
  n.begin(TEMP_HIGH); n.finish(Outcome::Success, 7100);
  CHECK(n.candidate(15000, TEMP_HIGH, true, s) == Reason::None);
  CHECK(n.candidate(7100 + 30 * 60000, TEMP_HIGH, true, s) == Reason::Reminder);
  CHECK(n.candidate(20000, 0, true, s) == Reason::Recovery);
  CHECK(n.candidate(20000, HUM_HIGH, true, s) == Reason::Changed);
  // Offline state changes are coalesced, never queued as historical alerts.
  CHECK(n.candidate(30000, HUM_HIGH, false, s) == Reason::None);
  CHECK(n.candidate(40000, 0, false, s) == Reason::None);
  CHECK(n.candidate(90000, 0, true, s) == Reason::Recovery);
  n.begin(0); n.finish(Outcome::Success, 90001);
  CHECK(n.candidate(90002, 0, true, s) == Reason::None);
  CHECK(n.candidate(90001 + 30 * 60000, 0, true, s) == Reason::Periodic);
  s.reportMinutes = 0; CHECK(n.candidate(90001 + 60 * 60000, 0, true, s) == Reason::None);
  n.candidate(4000000, 0, false, s);
  CHECK(n.candidate(4100000, 0, true, s) == Reason::Reconnected);
  // The remote mask is the state actually sent, including changes during HTTPS.
  n.begin(TEMP_HIGH); n.finish(Outcome::Success, 4200000);
  CHECK(n.candidate(4205000, HUM_HIGH, true, s) == Reason::Changed);
  std::puts("startup, periodic, reminder, recovery, coalesced offline state, in-flight changes: PASS");
}

static void failureBackoff() {
  using namespace thermo;
  CHECK(classifyHttp(200) == Outcome::Success);
  CHECK(classifyHttp(204) == Outcome::Permanent);
  CHECK(classifyHttp(302) == Outcome::Permanent);
  CHECK(classifyHttp(404) == Outcome::Permanent);
  CHECK(classifyHttp(429) == Outcome::RateLimited);
  CHECK(classifyHttp(503) == Outcome::Transient);
  CHECK(classifyHttp(0) == Outcome::Transient);
  CHECK(secondsToMs(1.0001) == 1001);
  CHECK(secondsToMs(65.5) == 65500);
  CHECK(secondsToMs(std::numeric_limits<double>::quiet_NaN()) == 0);
  Notifications n;
  n.begin(0); n.finish(Outcome::RateLimited, 1000, secondsToMs(3600));
  CHECK(n.nextAttempt == 3601000);
  n.reconfigure();
  CHECK(n.nextAttempt == 3601000 && !n.busy && !n.delivered && !n.blocked);
  CHECK(n.candidate(3600999, 0, true, Settings{}) == Reason::None);
  n.failures = 1;
  n.begin(0); n.finish(Outcome::Transient, 3601000);
  CHECK(n.nextAttempt == 3631000);
  n.begin(0); n.finish(Outcome::Transient, 3631000);
  CHECK(n.nextAttempt == 4531000 && n.failures == 0);
  n.begin(0); n.finish(Outcome::Permanent, 4531000);
  CHECK(n.blocked && n.candidate(9000000, 0, true, Settings{}) == Reason::None);
  n = Notifications{}; n.begin(0); n.finish(Outcome::Success, 1000, 20000);
  CHECK(n.nextAttempt == 21000);
  std::puts("confirmed HTTP success, invalid webhook stop, 429 seconds, server waits, bounded retry bursts: PASS");
}

static void historyPersistence() {
  using namespace thermo;
  History original; uint32_t epoch = 1780000000;
  for (size_t i = 0; i < 130; ++i) CHECK(original.append({epoch + uint32_t(i) * 720, 20 + float(i) / 10, 50}));
  CHECK(original.count == 120 && original.points[0].epoch == epoch + 7200);
  CHECK(!original.append({original.points[119].epoch, 25, 50}));
  uint8_t bytes[HISTORY_BYTES]; original.encode(bytes, 123);
  History restored; uint32_t generation = 0;
  CHECK(restored.decode(bytes, sizeof(bytes), generation));
  CHECK(generation == 123 && restored.count == original.count);
  for (size_t i = 0; i < 120; ++i) {
    CHECK(restored.points[i].epoch == original.points[i].epoch);
    CHECK(restored.points[i].temperature == original.points[i].temperature);
    CHECK(restored.points[i].humidity == original.points[i].humidity);
    CHECK(restored.points[i].startsSegment == original.points[i].startsSegment);
  }
  History untouched = restored;
  bytes[100] ^= 1; CHECK(!restored.decode(bytes, sizeof(bytes), generation));
  CHECK(restored.count == untouched.count && restored.points[0].epoch == untouched.points[0].epoch);
  CHECK(!restored.decode(bytes, sizeof(bytes) - 1, generation));
  CHECK(!restored.decode(nullptr, 0, generation));
  original.encode(bytes, 124);
  put32(bytes + 12 + 4, 0x7fc00000); put32(bytes + HISTORY_BYTES - 4, crc32(bytes, HISTORY_BYTES - 4));
  CHECK(!restored.decode(bytes, sizeof(bytes), generation));
  CHECK(newerGeneration(124, 123)); CHECK(newerGeneration(0, UINT32_MAX));
  CHECK(!newerGeneration(123, 124));
  CHECK(restored.pruneFuture(epoch + 100 * 720));
  CHECK(restored.points[restored.count - 1].epoch == epoch + 100 * 720);
  CHECK(crc32(reinterpret_cast<const uint8_t *>("123456789"), 9) == 0xcbf43926U);
  Point a{epoch, 20, 50}, normal{epoch + 720, 21, 51}, missing{epoch + 1440, 22, 52};
  CHECK(connectedPoints(a, normal)); CHECK(!connectedPoints(a, missing)); CHECK(!connectedPoints(a, a));
  Point reboot{epoch + 600, 21, 51, true};
  CHECK(!connectedPoints(a, reboot)); // Even a short power outage must leave a visible break.
  History segmented; CHECK(segmented.append(a)); CHECK(segmented.append(reboot));
  segmented.encode(bytes, 125); CHECK(restored.decode(bytes, sizeof(bytes), generation));
  CHECK(restored.points[1].startsSegment && !connectedPoints(restored.points[0], restored.points[1]));
  CHECK(graphX(epoch, epoch + 24 * 3600, 24 * 3600, 29, 127) == 29);
  CHECK(graphX(epoch + 12 * 3600, epoch + 24 * 3600, 24 * 3600, 29, 127) == 78);
  CHECK(graphX(epoch + 24 * 3600, epoch + 24 * 3600, 24 * 3600, 29, 127) == 127);
  std::puts("120-point rotation, full restoration, partial/corrupt saves, generation wrap, timestamp gaps: PASS");
}

static void configurationProtocol() {
  using namespace thermo;
  const char *validUrl = "https://discord.com/api/webhooks/123456789012345678/PLACEHOLDER_NOT_A_REAL_TOKEN_123456";
  CHECK(validWebhook(validUrl));
  CHECK(!validWebhook("https://discord.com.evil.example/api/webhooks/123456789012345678/token"));
  CHECK(!validWebhook("http://discord.com/api/webhooks/123456789012345678/PLACEHOLDER_NOT_A_REAL_TOKEN_123456"));
  CHECK(!validWebhook((std::string(validUrl) + "?wait=false").c_str()));
  Configuration c;
  cJSON *node = cJSON_Parse("{\"report_minutes\":60,\"screen_always_on\":false,\"temp_high\":31.5}");
  CHECK(configurationFromJson(node, c)); cJSON_Delete(node);
  CHECK(c.settings.reportMinutes == 60 && !c.settings.screenAlwaysOn && c.settings.tempHigh == 31.5f);
  const char *bad[] = {"{\"report_minutes\":-1}", "{\"report_minutes\":1.5}", "{\"temp_high\":null}",
    "{\"buzzer_enabled\":\"false\"}", "{\"wifi_password\":\"short\"}", "{\"unknown\":1}",
    "{\"report_minutes\":30,\"report_minutes\":10}", "{\"name\":\"\"}", "{\"name\":\"bad\\nname\"}", "[]"};
  for (const char *json : bad) {
    node = cJSON_Parse(json); CHECK(!configurationFromJson(node, c)); cJSON_Delete(node);
    CHECK(c.settings.reportMinutes == 60); // Invalid transactions do not partly alter configuration.
  }
  std::snprintf(c.ssid, sizeof(c.ssid), "PRIVATE_SSID");
  std::snprintf(c.password, sizeof(c.password), "PRIVATE_PASSWORD");
  std::snprintf(c.webhook, sizeof(c.webhook), "%s", validUrl);
  node = configurationToJson(c, false); char *publicJson = cJSON_PrintUnformatted(node);
  CHECK(!std::strstr(publicJson, "PRIVATE_SSID") && !std::strstr(publicJson, "PRIVATE_PASSWORD"));
  CHECK(!std::strstr(publicJson, "webhook_url") && !std::strstr(publicJson, "wifi_password"));
  cJSON_free(publicJson); cJSON_Delete(node);
  node = configurationToJson(c, true); Configuration restored;
  CHECK(configurationFromJson(node, restored)); cJSON_Delete(node);
  CHECK(restored.connectedSettings() && !std::strcmp(c.password, restored.password));
  std::puts("strict partial configuration, malformed/duplicate inputs, canonical host, secret-free status: PASS");
}

int main() {
  sensorAndSettings(); alarmBoundaries(); notificationLifecycle(); failureBackoff(); historyPersistence(); configurationProtocol();
  std::printf("PASS: %d assertions, 6 scenario groups\n", assertions);
}
