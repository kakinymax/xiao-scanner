#pragma once
#include "thermo_core.h"
#include <cJSON.h>
#include <cstdio>

namespace thermo {
struct Configuration {
  Settings settings;
  char ssid[33]{}, password[64]{}, webhook[256]{};
  bool connectedSettings() const { return ssid[0] && validWebhook(webhook); }
};

inline bool configurationFromJson(const cJSON *node, Configuration &out) {
  if (!cJSON_IsObject(node)) return false;
  Configuration candidate = out;
  for (const cJSON *item = node->child; item; item = item->next) {
    if (!item->string) return false;
    // Reject duplicate names as well as unknown settings.
    for (const cJSON *earlier = node->child; earlier != item; earlier = earlier->next)
      if (!std::strcmp(earlier->string, item->string)) return false;
    const char *key = item->string;
    char *dest = nullptr; size_t capacity = 0;
    if (!std::strcmp(key, "name")) { dest = candidate.settings.name; capacity = sizeof(candidate.settings.name); }
    else if (!std::strcmp(key, "ssid")) { dest = candidate.ssid; capacity = sizeof(candidate.ssid); }
    else if (!std::strcmp(key, "wifi_password")) { dest = candidate.password; capacity = sizeof(candidate.password); }
    else if (!std::strcmp(key, "webhook_url")) { dest = candidate.webhook; capacity = sizeof(candidate.webhook); }
    if (dest) {
      if (!cJSON_IsString(item) || !item->valuestring || std::strlen(item->valuestring) >= capacity) return false;
      for (const unsigned char *p = reinterpret_cast<const unsigned char *>(item->valuestring); *p; ++p)
        if (*p < 32 || *p == 127) return false;
      std::snprintf(dest, capacity, "%s", item->valuestring);
      continue;
    }
    bool *flag = nullptr;
    if (!std::strcmp(key, "temp_enabled")) flag = &candidate.settings.tempEnabled;
    else if (!std::strcmp(key, "humidity_enabled")) flag = &candidate.settings.humEnabled;
    else if (!std::strcmp(key, "buzzer_enabled")) flag = &candidate.settings.buzzerEnabled;
    else if (!std::strcmp(key, "screen_always_on")) flag = &candidate.settings.screenAlwaysOn;
    if (flag) { if (!cJSON_IsBool(item)) return false; *flag = cJSON_IsTrue(item); continue; }
    uint32_t *integer = nullptr;
    if (!std::strcmp(key, "sample_seconds")) integer = &candidate.settings.sampleSeconds;
    else if (!std::strcmp(key, "report_minutes")) integer = &candidate.settings.reportMinutes;
    else if (!std::strcmp(key, "save_minutes")) integer = &candidate.settings.saveMinutes;
    else if (!std::strcmp(key, "hold_seconds")) integer = &candidate.settings.holdSeconds;
    else if (!std::strcmp(key, "cooldown_minutes")) integer = &candidate.settings.repeatMinutes;
    if (integer) {
      if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) || item->valuedouble < 0 ||
          item->valuedouble > 144000 || std::floor(item->valuedouble) != item->valuedouble) return false;
      *integer = uint32_t(item->valuedouble); continue;
    }
    float *number = nullptr;
    if (!std::strcmp(key, "temp_high")) number = &candidate.settings.tempHigh;
    else if (!std::strcmp(key, "humidity_high")) number = &candidate.settings.humHigh;
    else if (!std::strcmp(key, "temp_hysteresis")) number = &candidate.settings.tempHysteresis;
    else if (!std::strcmp(key, "humidity_hysteresis")) number = &candidate.settings.humHysteresis;
    if (!number || !cJSON_IsNumber(item) || !std::isfinite(item->valuedouble)) return false;
    *number = float(item->valuedouble);
  }
  if (!validSettings(candidate.settings) || (candidate.webhook[0] && !validWebhook(candidate.webhook))) return false;
  size_t passLength = std::strlen(candidate.password);
  if (passLength && passLength < 8) return false;
  out = candidate;
  return true;
}

inline cJSON *configurationToJson(const Configuration &c, bool includeSecrets) {
  cJSON *o = cJSON_CreateObject();
  if (!o) return nullptr;
  cJSON_AddStringToObject(o, "name", c.settings.name);
  cJSON_AddNumberToObject(o, "sample_seconds", c.settings.sampleSeconds);
  cJSON_AddNumberToObject(o, "report_minutes", c.settings.reportMinutes);
  cJSON_AddNumberToObject(o, "save_minutes", c.settings.saveMinutes);
  cJSON_AddNumberToObject(o, "hold_seconds", c.settings.holdSeconds);
  cJSON_AddNumberToObject(o, "cooldown_minutes", c.settings.repeatMinutes);
  cJSON_AddNumberToObject(o, "temp_high", c.settings.tempHigh);
  cJSON_AddNumberToObject(o, "humidity_high", c.settings.humHigh);
  cJSON_AddNumberToObject(o, "temp_hysteresis", c.settings.tempHysteresis);
  cJSON_AddNumberToObject(o, "humidity_hysteresis", c.settings.humHysteresis);
  cJSON_AddBoolToObject(o, "temp_enabled", c.settings.tempEnabled);
  cJSON_AddBoolToObject(o, "humidity_enabled", c.settings.humEnabled);
  cJSON_AddBoolToObject(o, "buzzer_enabled", c.settings.buzzerEnabled);
  cJSON_AddBoolToObject(o, "screen_always_on", c.settings.screenAlwaysOn);
  if (includeSecrets) {
    cJSON_AddStringToObject(o, "ssid", c.ssid);
    cJSON_AddStringToObject(o, "wifi_password", c.password);
    cJSON_AddStringToObject(o, "webhook_url", c.webhook);
  }
  return o;
}
} // namespace thermo
