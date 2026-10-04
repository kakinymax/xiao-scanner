#include "app.h"
#include "device_config.h"
#include "daily_core.h"
#include "network_worker.h"
#include "sensor_driver.h"
#include <Arduino.h>
#include <Preferences.h>
#include <RTClib.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <atomic>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace {
constexpr int BUTTON_PIN = 3, BUZZER_PIN = 5;
thermo::Configuration config;
thermo::History history;
thermo::Gate localHigh, localLow, localSensor;
thermo::Notifications notifications;
thermo::Notifications graphNotifications;
thermo::DailyState dailyState;
bool stateDirty = false, stateOk = true, stateCorrupt = false, sending = false;
uint32_t stateGeneration = 0;
int stateSlot = 0;
uint64_t nextStateSave = 0;
SensorDriver sensor;
RTC_PCF8563 rtc;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
bool rtcPresent = false, oledPresent = false, workerReady = false;
bool readingValid = false, readingAttempted = false, historyDirty = false, storageOk = true, configOk = true;
bool wifiTrying = false, wifiWasConnected = false, ntpSeen = false;
bool sendAttempted = false;
bool newHistorySegment = true;
bool displayOn = true, buttonRaw = false, buttonStable = false;
bool buttonWokeScreen = false;
float temperature = 0, humidity = 0;
uint32_t readingEpoch = 0, nextHistoryEpoch = 0, historyGeneration = 0, configGeneration = 1;
int historySlot = 0, lastHttpStatus = 0, displayMode = 0, rangeIndex = 0;
uint8_t buzzerMask = 0, pulsesLeft = 0;
uint64_t nextSample = 0, nextSave = 0, nextDraw = 0, screenUntil = 30000;
uint64_t wifiDeadline = 0, wifiRetryAt = 0, wifiBackoff = 15000;
uint64_t rawChangedAt = 0, buttonPressedAt = 0, pulseAt = 0, lastBuzz = 0;
uint64_t readingAt = 0;
std::atomic<bool> ntpUpdated{false};

uint64_t uptime() { return uint64_t(esp_timer_get_time()) / 1000; }
uint32_t clockEpoch() {
  time_t t = time(nullptr);
  return t > 0 && uint64_t(t) <= UINT32_MAX && thermo::validEpoch(uint32_t(t)) ? uint32_t(t) : 0;
}
void timeText(uint32_t epoch, char *out, size_t size, const char *format) {
  if (!epoch) { std::snprintf(out, size, "--"); return; }
  time_t t = epoch; tm local{}; localtime_r(&t, &local); strftime(out, size, format, &local);
}
void onNtpSync(timeval *) { ntpUpdated.store(true); }

bool saveConfig(const thermo::Configuration &candidate) {
  cJSON *object = thermo::configurationToJson(candidate, true);
  char *json = object ? cJSON_PrintUnformatted(object) : nullptr;
  bool success = false;
  if (json) {
    Preferences prefs;
    if (prefs.begin("thermo_cfg", false)) {
      success = prefs.putString("cfg1", json) == std::strlen(json);
      prefs.end();
    }
    std::memset(json, 0, std::strlen(json)); cJSON_free(json);
  }
  cJSON_Delete(object); return success;
}
void loadConfig() {
  Preferences prefs;
  if (!prefs.begin("thermo_cfg", false)) { configOk = false; return; }
  String json = prefs.getString("cfg1", ""); prefs.end();
  if (json.isEmpty()) return;
  cJSON *object = cJSON_Parse(json.c_str());
  configOk = object && thermo::configurationFromJson(object, config);
  bool migrate = configOk && !cJSON_GetObjectItemCaseSensitive(object, "policy_version");
  cJSON_Delete(object);
  if (migrate) {
    config.settings.tempHigh = 40; config.settings.tempLow = 0;
    config.settings.reportMinutes = 0; config.settings.humEnabled = false; config.settings.repeatMinutes = 60;
    configOk = saveConfig(config); // Preserve the existing Wi-Fi/graph URL and history.
  }
}
void loadHistory() {
  Preferences prefs;
  if (!prefs.begin("thermo_hist", false)) { storageOk = false; return; }
  thermo::History candidate; uint32_t generation = 0; bool found = false, anyStored = false;
  static uint8_t bytes[thermo::HISTORY_BYTES];
  for (int i = 0; i < 2; ++i) {
    const char *key = i ? "hist1" : "hist0";
    size_t size = prefs.getBytesLength(key); anyStored |= size > 0;
    if (size == sizeof(bytes) && prefs.getBytes(key, bytes, sizeof(bytes)) == sizeof(bytes) &&
        candidate.decode(bytes, sizeof(bytes), generation)) {
      if (!found || thermo::newerGeneration(generation, historyGeneration)) {
        history = candidate; historyGeneration = generation; historySlot = i; found = true;
      }
    }
  }
  prefs.end();
  if (!found && anyStored) storageOk = false;
}
bool saveHistory() {
  uint8_t bytes[thermo::HISTORY_BYTES];
  history.encode(bytes, historyGeneration + 1);
  Preferences prefs; bool success = false;
  if (prefs.begin("thermo_hist", false)) {
    success = prefs.putBytes(historySlot ? "hist0" : "hist1", bytes, sizeof(bytes)) == sizeof(bytes);
    prefs.end();
  }
  if (success) { ++historyGeneration; historySlot = 1 - historySlot; historyDirty = false; }
  storageOk = success; return success;
}

void loadDailyState() {
  Preferences prefs;
  if (!prefs.begin("thermo_daily", false)) { stateOk = false; return; }
  static uint8_t bytes[thermo::DAILY_STATE_BYTES];
  static thermo::DailyState candidate;
  uint32_t generation = 0; bool any = false, found = false;
  for (int i = 0; i < 2; ++i) {
    const char *key = i ? "day1" : "day0";
    size_t size = prefs.getBytesLength(key); any |= size > 0;
    if (size == sizeof(bytes) && prefs.getBytes(key, bytes, sizeof(bytes)) == sizeof(bytes) &&
        candidate.decode(bytes, sizeof(bytes), generation) && (!found || thermo::newerGeneration(generation, stateGeneration))) {
      dailyState = candidate; stateGeneration = generation; stateSlot = i; found = true;
    }
  }
  prefs.end(); if (any && !found) { stateOk = false; stateCorrupt = true; }
}
bool saveDailyState() {
  static uint8_t bytes[thermo::DAILY_STATE_BYTES];
  dailyState.encode(bytes, stateGeneration + 1);
  Preferences prefs; bool success = false;
  if (prefs.begin("thermo_daily", false)) {
    success = prefs.putBytes(stateSlot ? "day0" : "day1", bytes, sizeof(bytes)) == sizeof(bytes);
    prefs.end();
  }
  if (success) { ++stateGeneration; stateSlot = 1 - stateSlot; stateDirty = false; }
  stateOk = success; return success;
}
uint8_t localMask() {
  return (localHigh.active ? thermo::TEMP_HIGH : 0) | (localLow.active ? thermo::TEMP_LOW : 0) |
         (localSensor.active ? thermo::SENSOR_ERROR : 0);
}
void handleDailyState(uint64_t now) {
  if (stateCorrupt) return; // Do not erase a damaged notification record and re-send old episodes.
  uint32_t epoch = clockEpoch();
  if (dailyState.daily.advance(epoch, history)) stateDirty = true;
  if (readingAttempted && epoch && (!readingValid || readingEpoch) && now - readingAt <= uint64_t(config.settings.sampleSeconds) * 2000) {
    uint32_t revision = dailyState.monitor.revision;
    if (dailyState.monitor.observe(epoch, temperature, humidity, readingValid && readingEpoch,
        config.settings.tempEnabled, config.settings.tempHigh, config.settings.tempLow)) stateDirty = true;
    if (revision != dailyState.monitor.revision) dailyState.monitor.measuredEpoch = readingValid ? readingEpoch : 0;
  }
  if (stateDirty && now >= nextStateSave) nextStateSave = now + (saveDailyState() ? 0 : 60000);
}

void handleClock() {
  if (!ntpUpdated.exchange(false)) return;
  uint32_t epoch = clockEpoch();
  if (!epoch) return;
  uint32_t expected = readingEpoch ? readingEpoch + uint32_t((uptime() - readingAt) / 1000) : 0;
  if (!expected || std::abs(int64_t(epoch) - expected) > 5) newHistorySegment = true;
  ntpSeen = true;
  if (rtcPresent) rtc.adjust(DateTime(epoch)); // RTC stores UTC; JST is for presentation only.
  if (history.pruneFuture(epoch)) { historyDirty = true; nextHistoryEpoch = 0; }
  // Re-measure after a time correction instead of attaching a new date to an old reading.
  nextSample = 0;
}
void handleWifi(uint64_t now) {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    wifiTrying = false; wifiBackoff = 15000;
    if (!wifiWasConnected) {
      configTzTime("JST-9", "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      wifiWasConnected = true;
    }
    return;
  }
  wifiWasConnected = false;
  if (!config.ssid[0]) return;
  if (wifiTrying && now >= wifiDeadline) {
    WiFi.disconnect(); wifiTrying = false; wifiRetryAt = now + wifiBackoff;
    wifiBackoff = wifiBackoff < 150000 ? wifiBackoff * 2 : 300000;
  }
  if (!wifiTrying && now >= wifiRetryAt) {
    WiFi.begin(config.ssid, config.password);
    wifiTrying = true; wifiDeadline = now + 20000;
  }
}

void stopBuzz() { noTone(BUZZER_PIN); pulsesLeft = 0; }
void handleBuzzer(uint64_t now) {
  uint8_t mask = localMask() & (thermo::TEMP_HIGH | thermo::TEMP_LOW);
  if (!mask || !config.settings.buzzerEnabled) {
    if (buzzerMask || pulsesLeft) stopBuzz();
    buzzerMask = 0; return;
  }
  if ((mask & ~buzzerMask) || now - lastBuzz >= uint64_t(config.settings.repeatMinutes) * 60000) {
    lastBuzz = now; pulsesLeft = 3; pulseAt = now; screenUntil = now + 30000;
  }
  buzzerMask = mask;
  if (pulsesLeft && now >= pulseAt) {
    tone(BUZZER_PIN, 2200, 150); --pulsesLeft; pulseAt = now + 350;
  }
}
void completeMeasurement(bool valid, float t, float h, uint64_t now) {
  readingValid = valid;
  readingAttempted = true;
  readingAt = now;
  if (!valid) newHistorySegment = true;
  readingEpoch = valid ? clockEpoch() : 0;
  if (valid) { temperature = t; humidity = h; }
  uint64_t hold = uint64_t(config.settings.holdSeconds) * 1000;
  localSensor.observe(!valid, true, now, hold);
  if (!config.settings.tempEnabled) { localHigh = thermo::Gate{}; localLow = thermo::Gate{}; }
  else {
    localHigh.observe(t > config.settings.tempHigh, valid, now, hold);
    localLow.observe(t < config.settings.tempLow, valid, now, hold);
  }
  if (valid && readingEpoch) {
    if (history.pruneFuture(readingEpoch)) { historyDirty = true; nextHistoryEpoch = 0; }
    if (!nextHistoryEpoch && history.count)
      nextHistoryEpoch = (history.points[history.count - 1].epoch / thermo::HISTORY_STEP_SECONDS + 1) * thermo::HISTORY_STEP_SECONDS;
    if (readingEpoch >= nextHistoryEpoch) {
      if (history.append({readingEpoch, t, h, newHistorySegment})) {
        historyDirty = true; newHistorySegment = false;
      }
      nextHistoryEpoch = (readingEpoch / thermo::HISTORY_STEP_SECONDS + 1) * thermo::HISTORY_STEP_SECONDS;
    }
  }
  nextSample = now + uint64_t(config.settings.sampleSeconds) * 1000;
  nextDraw = 0;
}
void handleSensor(uint64_t now) {
  float t = 0, h = 0; bool valid = false;
  if (sensor.active()) {
    if (sensor.poll(now, t, h, valid)) completeMeasurement(valid, t, h, now);
  } else if (now >= nextSample) {
    if (!sensor.start(now)) completeMeasurement(false, 0, 0, now);
  }
}

bool sendNotification(JobKind kind) {
  if (!workerReady || sending) return false;
  bool graph = kind == JobKind::Daily || kind == JobKind::GraphTest;
  static WebhookJob job; // Copied into the worker queue; keep the 3.5 KB job off the loop stack.
  job.history.count = 0; job.token = job.begin = job.end = 0;
  job.generation = configGeneration; job.kind = kind;
  std::snprintf(job.url, sizeof(job.url), "%s", graph ? config.webhook : config.alertWebhook);
  if (!thermo::validWebhook(job.url)) return false;
  char message[800], measured[40], sent[40], values[100], state[120];
  timeText(clockEpoch(), sent, sizeof(sent), "%Y/%m/%d %H:%M:%S JST");
  uint8_t mask = localMask();
  if (graph) {
    if (kind == JobKind::Daily) {
      job.token = dailyState.daily.targetDay; job.begin = thermo::dayStart(job.token); job.end = job.begin + 86400;
      job.history = dailyState.daily.frozen;
    } else {
      job.end = clockEpoch(); job.begin = job.end - 86400;
      for (size_t i = 0; i < history.count; ++i)
        if (history.points[i].epoch >= job.begin && history.points[i].epoch < job.end) job.history.append(history.points[i]);
    }
    char begin[40], end[40];
    timeText(job.begin, begin, sizeof(begin), "%Y/%m/%d %H:%M");
    timeText(job.end, end, sizeof(end), "%Y/%m/%d %H:%M");
    std::snprintf(job.rangeLabel, sizeof(job.rangeLabel), "%s - %s JST", begin, end);
    std::snprintf(message, sizeof(message), "【%s】%s\n対象: %s〜%s JST\n上: 温度 / 下: 湿度\n記録: %u点（12分間隔、欠測・電源断は空白）",
      config.settings.name, kind == JobKind::Daily ? "1日の温湿度グラフ" : "グラフの手動送信確認（直近24時間）",
      begin, end, unsigned(job.history.count));
  } else {
    const auto &monitor = dailyState.monitor;
    bool test = kind == JobKind::AlertTest;
    bool valid = test ? readingValid : !(monitor.pendingMask & thermo::SENSOR_ERROR) && monitor.measuredEpoch;
    float t = test ? temperature : monitor.temperature, h = test ? humidity : monitor.humidity;
    mask = test ? localMask() : monitor.pendingMask; job.token = monitor.revision;
    if (valid) {
      std::snprintf(values, sizeof(values), "温度 %.1f℃ / 湿度 %.1f%%", t, h);
      timeText(test ? readingEpoch : monitor.measuredEpoch, measured, sizeof(measured), "%Y/%m/%d %H:%M:%S JST");
    } else {
      std::snprintf(values, sizeof(values), "センサーの現在値を取得できません");
      std::snprintf(measured, sizeof(measured), "未取得");
    }
    std::snprintf(state, sizeof(state), "%s%s%s%s", mask ? "条件: " : "状態: 通常",
      mask & thermo::TEMP_HIGH ? "高温 " : "", mask & thermo::TEMP_LOW ? "低温 " : "",
      mask & thermo::SENSOR_ERROR ? "センサー応答なし" : "");
    const char *reason = test ? "異常通知先の手動送信確認" : monitor.pending == thermo::AlertEvent::Recovery ?
      "正常な状態への復帰" : monitor.pending == thermo::AlertEvent::Changed ? "異常状態の変化" : "温度条件の超過・悪化";
    std::snprintf(message, sizeof(message), "【%s】%s\n%s\n%s\n測定: %s\n通知: %s\n監視: 1時間ごと / 高温 %.1f℃超・低温 %.1f℃未満",
      config.settings.name, reason, values, state, measured, sent, config.settings.tempHigh, config.settings.tempLow);
  }
  cJSON *body = cJSON_CreateObject();
  if (!body) return false;
  cJSON_AddStringToObject(body, "content", message);
  cJSON *mentions = cJSON_AddObjectToObject(body, "allowed_mentions");
  cJSON_AddArrayToObject(mentions, "parse");
  if (graph) {
    cJSON *embeds = cJSON_AddArrayToObject(body, "embeds");
    cJSON *embed = cJSON_CreateObject(); cJSON_AddItemToArray(embeds, embed);
    cJSON *image = cJSON_AddObjectToObject(embed, "image");
    cJSON_AddStringToObject(image, "url", "attachment://trend.png");
  }
  char *json = cJSON_PrintUnformatted(body);
  bool accepted = false;
  if (json && std::strlen(json) < sizeof(job.payload)) {
    std::snprintf(job.payload, sizeof(job.payload), "%s", json); accepted = submitWebhook(job);
  }
  cJSON_free(json); cJSON_Delete(body);
  if (accepted) { (graph ? graphNotifications : notifications).begin(mask); sending = true; }
  return accepted;
}
void handleNotifications(uint64_t now) {
  WebhookResult result;
  if (receiveWebhookResult(result)) {
    sending = false;
    bool graph = result.kind == JobKind::Daily || result.kind == JobKind::GraphTest;
    auto &delivery = graph ? graphNotifications : notifications;
    if (result.generation == configGeneration) {
      lastHttpStatus = result.httpStatus; sendAttempted = true;
      auto outcome = thermo::classifyHttp(result.httpStatus);
      delivery.finish(outcome, now, result.waitMs);
      // Discord may apply a global wait to both routes, so share the deadline.
      if (result.waitMs) {
        auto &other = graph ? notifications : graphNotifications;
        if (other.nextAttempt < now + result.waitMs) other.nextAttempt = now + result.waitMs;
      }
      if (outcome == thermo::Outcome::Success) {
        if (result.kind == JobKind::Daily) { dailyState.daily.complete(result.token); stateDirty = true; }
        if (result.kind == JobKind::Alert) { dailyState.monitor.complete(result.token); stateDirty = true; }
      }
    } else delivery.busy = false;
  }
  if (sending || stateDirty || !stateOk || !configOk || !workerReady || !clockEpoch() || WiFi.status() != WL_CONNECTED) return;
  if (dailyState.daily.targetDay && thermo::validWebhook(config.webhook) && !graphNotifications.blocked && now >= graphNotifications.nextAttempt) {
    if (!sendNotification(JobKind::Daily)) graphNotifications.finish(thermo::Outcome::Transient, now);
  } else if (dailyState.monitor.pending != thermo::AlertEvent::None && thermo::validWebhook(config.alertWebhook) &&
      !notifications.blocked && now >= notifications.nextAttempt) {
    if (!sendNotification(JobKind::Alert)) notifications.finish(thermo::Outcome::Transient, now);
  }
}

void handleButton(uint64_t now) {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  if (pressed != buttonRaw) { buttonRaw = pressed; rawChangedAt = now; }
  if (pressed == buttonStable || now - rawChangedAt < 30) return;
  buttonStable = pressed;
  if (pressed) { buttonWokeScreen = !displayOn; buttonPressedAt = now; screenUntil = now + 30000; }
  else {
    uint64_t duration = now - buttonPressedAt;
    screenUntil = now + 30000; stopBuzz();
    if (duration >= 1000) { if (!displayMode) displayMode = 1; rangeIndex = (rangeIndex + 1) % 3; }
    else if (!buttonWokeScreen) displayMode = (displayMode + 1) % 3;
    nextDraw = 0;
  }
}
void drawCurrent() {
  char label[32]; timeText(clockEpoch(), label, sizeof(label), "%H:%M:%S");
  oled.setFont(u8g2_font_5x7_tr); oled.drawStr(0, 7, label);
  oled.drawStr(92, 7, WiFi.status() == WL_CONNECTED ? "WiFi" : "OFF");
  if (readingValid) {
    oled.setFont(u8g2_font_ncenB12_tr);
    std::snprintf(label, sizeof(label), "T: %.1f C", temperature); oled.drawStr(0, 29, label);
    std::snprintf(label, sizeof(label), "H: %.1f %%", humidity); oled.drawStr(0, 50, label);
  } else {
    oled.setFont(u8g2_font_6x10_tr); oled.drawStr(0, 29, "Sensor unavailable");
  }
  oled.setFont(u8g2_font_5x7_tr);
  if (!configOk || !storageOk || !stateOk) oled.drawStr(0, 63, "Storage error: USB");
  else if (!config.connectedSettings()) oled.drawStr(0, 63, "Initial setup: USB");
  else if (notifications.blocked || graphNotifications.blocked) oled.drawStr(0, 63, "Send error: USB");
  else {
    std::snprintf(label, sizeof(label), "H:%u %s %s", unsigned(history.count),
      ntpSeen ? "NTP" : clockEpoch() ? "RTC" : "WAIT", localMask() ? "ALERT" : "OK");
    oled.drawStr(0, 63, label);
  }
}
void drawGraph() {
  uint32_t now = clockEpoch();
  constexpr int left = 29, right = 127, top = 15, bottom = 55;
  const uint32_t hours[] = {24, 12, 6}; uint32_t range = hours[rangeIndex] * 3600;
  char text[32], date[24];
  std::snprintf(text, sizeof(text), "%s %uh", displayMode == 1 ? "T" : "H", unsigned(hours[rangeIndex]));
  timeText(now, date, sizeof(date), "%m/%d %H:%M");
  oled.setFont(u8g2_font_5x7_tr); oled.drawStr(0, 7, text); oled.drawStr(72, 7, date);
  if (!now) { oled.drawStr(25, 35, "Clock wait"); return; }
  float low = 999, high = -999; size_t visible = 0;
  for (size_t i = 0; i < history.count; ++i) {
    const auto &p = history.points[i];
    if (p.epoch > now || now - p.epoch > range) continue;
    float v = displayMode == 1 ? p.temperature : p.humidity;
    if (v < low) low = v; if (v > high) high = v; ++visible;
  }
  if (!visible) { oled.drawStr(25, 35, "Collecting..."); return; }
  if (high - low < 2) { float middle = (low + high) / 2; low = middle - 1; high = middle + 1; }
  std::snprintf(text, sizeof(text), "%.1f", high); oled.drawStr(0, top + 3, text);
  std::snprintf(text, sizeof(text), "%.1f", low); oled.drawStr(0, bottom, text);
  oled.drawHLine(left, bottom, right - left + 1);
  oled.setFont(u8g2_font_4x6_tr);
  std::snprintf(text, sizeof(text), "%uh", unsigned(hours[rangeIndex])); oled.drawStr(left, 63, text);
  std::snprintf(text, sizeof(text), "%uh", unsigned(hours[rangeIndex] / 2)); oled.drawStr((left + right) / 2 - 4, 63, text);
  oled.drawStr(right - 8, 63, "0h");
  int oldX = 0, oldY = 0; bool previous = false; thermo::Point oldPoint;
  for (size_t i = 0; i < history.count; ++i) {
    const auto &p = history.points[i];
    if (p.epoch > now || now - p.epoch > range) { previous = false; continue; }
    int x = thermo::graphX(p.epoch, now, range, left, right);
    float value = displayMode == 1 ? p.temperature : p.humidity;
    int y = constrain(int(bottom - (value - low) * (bottom - top) / (high - low)), top, bottom);
    oled.drawPixel(x, y);
    if (previous && thermo::connectedPoints(oldPoint, p)) oled.drawLine(oldX, oldY, x, y);
    oldPoint = p; oldX = x; oldY = y; previous = true;
  }
}
void handleDisplay(uint64_t now) {
  if (!oledPresent) return;
  bool wanted = config.settings.screenAlwaysOn || buttonStable || now < screenUntil;
  if (wanted != displayOn) { displayOn = wanted; oled.setPowerSave(wanted ? 0 : 1); nextDraw = 0; }
  if (!displayOn || now < nextDraw) return;
  nextDraw = now + 1000; oled.clearBuffer();
  if (!displayMode) drawCurrent(); else drawGraph();
  oled.sendBuffer();
}

void reply(cJSON *object) {
  char *json = cJSON_PrintUnformatted(object);
  if (json) { Serial.println(json); cJSON_free(json); }
  cJSON_Delete(object);
}
cJSON *baseReply(int id, bool ok, const char *error = nullptr) {
  cJSON *r = cJSON_CreateObject(); cJSON_AddNumberToObject(r, "id", id); cJSON_AddBoolToObject(r, "ok", ok);
  if (error) cJSON_AddStringToObject(r, "error", error);
  return r;
}
void applyConfig(const thermo::Configuration &candidate, uint64_t now) {
  bool wifiChanged = std::strcmp(config.ssid, candidate.ssid) || std::strcmp(config.password, candidate.password);
  bool limitsChanged = config.settings.tempHigh != candidate.settings.tempHigh || config.settings.tempLow != candidate.settings.tempLow ||
    config.settings.tempEnabled != candidate.settings.tempEnabled;
  config = candidate; configOk = true; ++configGeneration;
  localHigh = localLow = localSensor = thermo::Gate{}; notifications.reconfigure(); graphNotifications.reconfigure();
  if (limitsChanged) { dailyState.monitor = thermo::HourlyMonitor{}; stateDirty = true; }
  stopBuzz(); buzzerMask = 0;
  nextSample = 0; nextSave = now + uint64_t(config.settings.saveMinutes) * 60000;
  screenUntil = now + 30000; nextDraw = 0; lastHttpStatus = 0; sendAttempted = false;
  if (wifiChanged) {
    WiFi.disconnect(); wifiTrying = false; wifiWasConnected = false; wifiRetryAt = now; wifiBackoff = 15000;
  }
}
void serialCommand(const char *line, uint64_t now) {
  const char *end = nullptr;
  cJSON *root = std::strstr(line, "\\u0000") ? nullptr : cJSON_ParseWithOpts(line, &end, true);
  int id = 0;
  cJSON *idNode = root ? cJSON_GetObjectItemCaseSensitive(root, "id") : nullptr;
  if (cJSON_IsNumber(idNode) && idNode->valuedouble >= 0 && idNode->valuedouble <= INT32_MAX &&
      std::floor(idNode->valuedouble) == idNode->valuedouble) id = int(idNode->valuedouble);
  cJSON *cmd = root ? cJSON_GetObjectItemCaseSensitive(root, "cmd") : nullptr;
  bool valid = cJSON_IsObject(root) && cJSON_IsString(cmd) && idNode && id > 0;
  if (valid) {
    for (cJSON *item = root->child; item; item = item->next) {
      if (std::strcmp(item->string, "cmd") && std::strcmp(item->string, "id") && std::strcmp(item->string, "settings")) valid = false;
      for (cJSON *earlier = root->child; earlier != item; earlier = earlier->next)
        if (!std::strcmp(earlier->string, item->string)) valid = false;
    }
  }
  if (!valid) { cJSON_Delete(root); reply(baseReply(id, false, "invalid_request")); return; }
  if (!std::strcmp(cmd->valuestring, "status")) {
    cJSON *r = baseReply(id, true);
    cJSON_AddStringToObject(r, "firmware", thermo::VERSION);
    cJSON_AddBoolToObject(r, "configured", config.connectedSettings());
    cJSON_AddBoolToObject(r, "alert_configured", thermo::validWebhook(config.alertWebhook));
    cJSON_AddBoolToObject(r, "wifi_connected", WiFi.status() == WL_CONNECTED);
    cJSON_AddStringToObject(r, "clock", ntpSeen ? "ntp" : clockEpoch() ? "rtc" : "none");
    cJSON_AddBoolToObject(r, "reading_valid", readingValid);
    if (readingValid) {
      cJSON_AddNumberToObject(r, "temperature", temperature); cJSON_AddNumberToObject(r, "humidity", humidity);
    }
    cJSON_AddNumberToObject(r, "alarm_mask", localMask());
    cJSON_AddNumberToObject(r, "history_points", history.count);
    cJSON_AddBoolToObject(r, "storage_ok", storageOk && configOk && stateOk);
    cJSON_AddNumberToObject(r, "daily_pending_day", dailyState.daily.targetDay);
    cJSON_AddNumberToObject(r, "daily_delivered_day", dailyState.daily.deliveredDay);
    cJSON_AddNumberToObject(r, "next_high_notification", dailyState.monitor.nextHigh);
    cJSON_AddNumberToObject(r, "next_low_notification", dailyState.monitor.nextLow);
    cJSON_AddBoolToObject(r, "worker_ready", workerReady);
    cJSON_AddNumberToObject(r, "http_status", lastHttpStatus);
    cJSON_AddBoolToObject(r, "send_attempted", sendAttempted);
    cJSON_AddBoolToObject(r, "send_blocked", notifications.blocked || graphNotifications.blocked);
    cJSON_AddBoolToObject(r, "sending", sending);
    cJSON_AddNumberToObject(r, "retry_seconds", notifications.nextAttempt > now ? (notifications.nextAttempt - now + 999) / 1000 : 0);
    cJSON_AddItemToObject(r, "settings", thermo::configurationToJson(config, false)); reply(r);
  } else if (!std::strcmp(cmd->valuestring, "configure") || !std::strcmp(cmd->valuestring, "clear_connection")) {
    thermo::Configuration candidate = config;
    if (sending) reply(baseReply(id, false, "busy_retry"));
    else {
      bool clear = !std::strcmp(cmd->valuestring, "clear_connection");
      bool parsed = true;
      if (clear) { candidate.ssid[0] = 0; candidate.password[0] = 0; candidate.webhook[0] = 0; candidate.alertWebhook[0] = 0; }
      else parsed = thermo::configurationFromJson(cJSON_GetObjectItemCaseSensitive(root, "settings"), candidate);
      if (!parsed) reply(baseReply(id, false, "invalid_settings"));
      else if (!saveConfig(candidate)) reply(baseReply(id, false, "storage_failed"));
      else { applyConfig(candidate, now); reply(baseReply(id, true)); }
    }
  } else if (!std::strcmp(cmd->valuestring, "test") || !std::strcmp(cmd->valuestring, "test_graph")) {
    bool graph = !std::strcmp(cmd->valuestring, "test_graph");
    auto &delivery = graph ? graphNotifications : notifications;
    if (!(graph ? thermo::validWebhook(config.webhook) : thermo::validWebhook(config.alertWebhook)) ||
        WiFi.status() != WL_CONNECTED || !clockEpoch() || !workerReady)
      reply(baseReply(id, false, "not_ready"));
    else if (sending) reply(baseReply(id, false, "busy_retry"));
    else if (now < delivery.nextAttempt) reply(baseReply(id, false, "retry_wait"));
    else {
      delivery.blocked = false;
      reply(baseReply(id, sendNotification(graph ? JobKind::GraphTest : JobKind::AlertTest)));
    }
  } else reply(baseReply(id, false, "unknown_command"));
  cJSON_Delete(root);
}
void handleSerial(uint64_t now) {
  static char line[1536]; static size_t used = 0; static bool overflow = false;
  // Bound processing per loop so a noisy USB peer cannot starve measurement/display.
  for (int i = 0; i < 128 && Serial.available(); ++i) {
    int c = Serial.read();
    if (c == '\n') {
      if (overflow) reply(baseReply(0, false, "request_too_long"));
      else if (used) { line[used] = 0; serialCommand(line, now); }
      std::memset(line, 0, sizeof(line)); used = 0; overflow = false;
    } else if (c != '\r') {
      if (!c || used >= sizeof(line) - 1) overflow = true;
      else if (!overflow) line[used++] = char(c);
    }
  }
}
} // namespace

void setupThermometer() {
  // HWCDC defaults to 256 bytes; a complete USB request can contain 1535 bytes.
  // Reserve the queues before begin so requests and public replies are not truncated.
  Serial.setRxBufferSize(2048);
  Serial.setTxBufferSize(2048);
  Serial.begin(115200); // USB CDC on boot must be enabled in the board options.
  pinMode(BUTTON_PIN, INPUT_PULLUP); pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  Wire.begin(); Wire.setTimeOut(50);
  setenv("TZ", "JST-9", 1); tzset();
  loadConfig(); loadHistory(); loadDailyState();
  rtcPresent = rtc.begin();
  if (rtcPresent && !rtc.lostPower() && rtc.isrunning()) {
    uint32_t epoch = rtc.now().unixtime();
    if (thermo::validEpoch(epoch)) { timeval tv{time_t(epoch), 0}; settimeofday(&tv, nullptr); }
  }
  uint32_t now = clockEpoch();
  if (now && history.pruneFuture(now)) historyDirty = true;
  Wire.beginTransmission(0x3c); oledPresent = Wire.endTransmission() == 0;
  if (oledPresent) { oled.begin(); oled.setPowerSave(0); }
  WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_STA); WiFi.setSleep(false);
  esp_sntp_set_time_sync_notification_cb(onNtpSync);
  workerReady = startWebhookWorker();
  nextSample = uptime() + 40; nextSave = uptime() + uint64_t(config.settings.saveMinutes) * 60000;
  screenUntil = uptime() + 30000;
}
void loopThermometer() {
  uint64_t now = uptime();
  handleSerial(now); handleWifi(now); handleClock(); handleDailyState(now); handleSensor(now);
  handleBuzzer(now); handleButton(now); handleNotifications(now);
  if (historyDirty && now >= nextSave)
    nextSave = now + (saveHistory() ? uint64_t(config.settings.saveMinutes) * 60000 : 60000);
  handleDisplay(now);
  delay(5);
}
