#include "app.h"
#include "device_config.h"
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
thermo::Alarms alarms;
thermo::Notifications notifications;
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
  cJSON_Delete(object);
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
  uint8_t mask = alarms.mask() & (thermo::TEMP_HIGH | thermo::HUM_HIGH);
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
  alarms.observe(t, h, valid, now, config.settings);
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

const char *reasonText(thermo::Reason reason) {
  switch (reason) {
  case thermo::Reason::Startup: return "起動・設定後の報告";
  case thermo::Reason::Alarm: return "設定した条件の超過";
  case thermo::Reason::Recovery: return "正常な状態への復帰";
  case thermo::Reason::Changed: return "異常状態の変化";
  case thermo::Reason::Reconnected: return "通信復帰後の現在の状態";
  case thermo::Reason::Reminder: return "異常状態の継続";
  case thermo::Reason::Test: return "手動の送信確認";
  default: return "定期報告";
  }
}
bool sendNotification(thermo::Reason reason) {
  if (!workerReady || notifications.busy) return false;
  WebhookJob job; job.generation = configGeneration;
  std::snprintf(job.url, sizeof(job.url), "%s", config.webhook);
  char values[100], measured[40], sent[40], state[160], message[800];
  timeText(clockEpoch(), sent, sizeof(sent), "%Y/%m/%d %H:%M:%S JST");
  if (readingValid) {
    std::snprintf(values, sizeof(values), "温度 %.1f℃ / 湿度 %.1f%%", temperature, humidity);
    timeText(readingEpoch, measured, sizeof(measured), "%Y/%m/%d %H:%M:%S JST");
  } else {
    std::snprintf(values, sizeof(values), "センサーの現在値を取得できません");
    std::snprintf(measured, sizeof(measured), "未取得");
  }
  uint8_t mask = alarms.mask();
  std::snprintf(state, sizeof(state), "%s%s%s%s", mask ? "条件: " : readingValid ? "状態: 通常" : "状態: センサー確認中",
    mask & thermo::TEMP_HIGH ? "高温 " : "", mask & thermo::HUM_HIGH ? "高湿度 " : "",
    mask & thermo::SENSOR_ERROR ? "センサー応答なし" : "");
  std::snprintf(message, sizeof(message), "【%s】%s\n%s\n%s\n測定: %s\n通知: %s",
    config.settings.name, reasonText(reason), values, state, measured, sent);
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "content", message);
  cJSON *mentions = cJSON_AddObjectToObject(body, "allowed_mentions");
  cJSON_AddArrayToObject(mentions, "parse");
  char *json = cJSON_PrintUnformatted(body);
  bool accepted = false;
  if (json && std::strlen(json) < sizeof(job.payload)) {
    std::snprintf(job.payload, sizeof(job.payload), "%s", json);
    accepted = submitWebhook(job);
  }
  cJSON_free(json); cJSON_Delete(body);
  if (accepted) notifications.begin(mask);
  return accepted;
}
void handleNotifications(uint64_t now) {
  WebhookResult result;
  if (receiveWebhookResult(result)) {
    if (result.generation == configGeneration) {
      lastHttpStatus = result.httpStatus;
      sendAttempted = true;
      notifications.finish(thermo::classifyHttp(result.httpStatus), now, result.waitMs);
    }
  }
  bool ready = readingAttempted && config.connectedSettings() && WiFi.status() == WL_CONNECTED && clockEpoch() && workerReady;
  thermo::Reason reason = notifications.candidate(now, alarms.mask(), ready, config.settings);
  if (reason != thermo::Reason::None && !sendNotification(reason))
    notifications.finish(thermo::Outcome::Transient, now);
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
  if (!configOk || !storageOk) oled.drawStr(0, 63, "Storage error: USB");
  else if (!config.connectedSettings()) oled.drawStr(0, 63, "Initial setup: USB");
  else if (notifications.blocked) oled.drawStr(0, 63, "Send error: USB");
  else {
    std::snprintf(label, sizeof(label), "H:%u %s %s", unsigned(history.count),
      ntpSeen ? "NTP" : clockEpoch() ? "RTC" : "WAIT", alarms.mask() ? "ALERT" : "OK");
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
  config = candidate; configOk = true; ++configGeneration;
  alarms = thermo::Alarms{}; notifications.reconfigure();
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
    cJSON_AddBoolToObject(r, "wifi_connected", WiFi.status() == WL_CONNECTED);
    cJSON_AddStringToObject(r, "clock", ntpSeen ? "ntp" : clockEpoch() ? "rtc" : "none");
    cJSON_AddBoolToObject(r, "reading_valid", readingValid);
    if (readingValid) {
      cJSON_AddNumberToObject(r, "temperature", temperature); cJSON_AddNumberToObject(r, "humidity", humidity);
    }
    cJSON_AddNumberToObject(r, "alarm_mask", alarms.mask());
    cJSON_AddNumberToObject(r, "history_points", history.count);
    cJSON_AddBoolToObject(r, "storage_ok", storageOk && configOk);
    cJSON_AddBoolToObject(r, "worker_ready", workerReady);
    cJSON_AddNumberToObject(r, "http_status", lastHttpStatus);
    cJSON_AddBoolToObject(r, "send_attempted", sendAttempted);
    cJSON_AddBoolToObject(r, "send_blocked", notifications.blocked);
    cJSON_AddBoolToObject(r, "sending", notifications.busy);
    cJSON_AddNumberToObject(r, "retry_seconds", notifications.nextAttempt > now ? (notifications.nextAttempt - now + 999) / 1000 : 0);
    cJSON_AddItemToObject(r, "settings", thermo::configurationToJson(config, false)); reply(r);
  } else if (!std::strcmp(cmd->valuestring, "configure") || !std::strcmp(cmd->valuestring, "clear_connection")) {
    thermo::Configuration candidate = config;
    if (notifications.busy) reply(baseReply(id, false, "busy_retry"));
    else {
      bool clear = !std::strcmp(cmd->valuestring, "clear_connection");
      bool parsed = true;
      if (clear) { candidate.ssid[0] = 0; candidate.password[0] = 0; candidate.webhook[0] = 0; }
      else parsed = thermo::configurationFromJson(cJSON_GetObjectItemCaseSensitive(root, "settings"), candidate);
      if (!parsed) reply(baseReply(id, false, "invalid_settings"));
      else if (!saveConfig(candidate)) reply(baseReply(id, false, "storage_failed"));
      else { applyConfig(candidate, now); reply(baseReply(id, true)); }
    }
  } else if (!std::strcmp(cmd->valuestring, "test")) {
    if (!readingAttempted || !config.connectedSettings() || WiFi.status() != WL_CONNECTED || !clockEpoch() || !workerReady)
      reply(baseReply(id, false, "not_ready"));
    else if (notifications.busy) reply(baseReply(id, false, "busy_retry"));
    else if (now < notifications.nextAttempt) reply(baseReply(id, false, "retry_wait"));
    else {
      notifications.blocked = false;
      reply(baseReply(id, sendNotification(thermo::Reason::Test)));
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
  Serial.begin(115200); // USB CDC on boot must be enabled in the board options.
  pinMode(BUTTON_PIN, INPUT_PULLUP); pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  Wire.begin(); Wire.setTimeOut(50);
  setenv("TZ", "JST-9", 1); tzset();
  loadConfig(); loadHistory();
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
  handleSerial(now); handleWifi(now); handleClock(); handleSensor(now);
  handleBuzzer(now); handleButton(now); handleNotifications(now);
  if (historyDirty && now >= nextSave)
    nextSave = now + (saveHistory() ? uint64_t(config.settings.saveMinutes) * 60000 : 60000);
  handleDisplay(now);
  delay(5);
}
