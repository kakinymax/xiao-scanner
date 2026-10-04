#pragma once
#include "thermo_core.h"
enum class JobKind : uint8_t { Alert, Daily, GraphTest, AlertTest };
#include "thermo_core.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct WebhookJob {
  char url[256]{};
  char payload[1200]{};
  uint32_t generation = 0;
  JobKind kind = JobKind::Alert;
  uint32_t token = 0, begin = 0, end = 0;
  thermo::History history;
  char rangeLabel[96]{};
};
struct WebhookResult {
  uint32_t generation = 0;
  int httpStatus = 0;
  uint64_t waitMs = 0;
  JobKind kind = JobKind::Alert;
  uint32_t token = 0;
};

// There is at most one in-flight request. All device I2C/display work stays in loop().
bool startWebhookWorker();
bool submitWebhook(const WebhookJob &job);
bool receiveWebhookResult(WebhookResult &result);
