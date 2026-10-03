#pragma once
#include "thermo_core.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct WebhookJob {
  char url[256]{};
  char payload[1200]{};
  uint32_t generation = 0;
};
struct WebhookResult {
  uint32_t generation = 0;
  int httpStatus = 0;
  uint64_t waitMs = 0;
};

// There is at most one in-flight request. All device I2C/display work stays in loop().
bool startWebhookWorker();
bool submitWebhook(const WebhookJob &job);
bool receiveWebhookResult(WebhookResult &result);
