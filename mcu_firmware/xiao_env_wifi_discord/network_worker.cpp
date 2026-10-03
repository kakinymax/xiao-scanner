#include "network_worker.h"
#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <cstdlib>
#include <cstdio>
#include <strings.h>

namespace {
QueueHandle_t jobs = nullptr, results = nullptr;
struct Response {
  char body[1024]{};
  size_t used = 0;
  double retry = 0, reset = 0;
  int remaining = -1;
};
double headerSeconds(const char *s) {
  if (!s) return 0;
  char *end = nullptr;
  double n = std::strtod(s, &end);
  return end != s && !*end && std::isfinite(n) && n > 0 ? n : 0;
}
esp_err_t onHttpEvent(esp_http_client_event_t *event) {
  auto &r = *static_cast<Response *>(event->user_data);
  if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
    size_t length = size_t(event->data_len);
    if (length > sizeof(r.body) - 1 - r.used) length = sizeof(r.body) - 1 - r.used;
    std::memcpy(r.body + r.used, event->data, length); r.used += length; r.body[r.used] = 0;
  } else if (event->event_id == HTTP_EVENT_ON_HEADER) {
    if (!strcasecmp(event->header_key, "Retry-After")) r.retry = headerSeconds(event->header_value);
    if (!strcasecmp(event->header_key, "X-RateLimit-Reset-After")) r.reset = headerSeconds(event->header_value);
    if (!strcasecmp(event->header_key, "X-RateLimit-Remaining")) r.remaining = std::atoi(event->header_value);
  }
  return ESP_OK;
}
void worker(void *) {
  WebhookJob job;
  for (;;) {
    if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
    WebhookResult result; result.generation = job.generation;
    Response response;
    if (!thermo::validWebhook(job.url)) result.httpStatus = 400;
    else {
      char url[272]; std::snprintf(url, sizeof(url), "%s?wait=true", job.url);
      esp_http_client_config_t config{};
      config.url = url; config.method = HTTP_METHOD_POST;
      config.timeout_ms = 8000;
      config.crt_bundle_attach = esp_crt_bundle_attach;
      config.disable_auto_redirect = true;
      config.event_handler = onHttpEvent; config.user_data = &response;
      config.buffer_size = 1024; config.buffer_size_tx = 1536;
      esp_http_client_handle_t client = esp_http_client_init(&config);
      if (client) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "User-Agent", "XIAO-Temperature/1.0");
        esp_http_client_set_post_field(client, job.payload, std::strlen(job.payload));
        if (esp_http_client_perform(client) == ESP_OK) result.httpStatus = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
      }
      if (result.httpStatus == 429) {
        cJSON *body = cJSON_Parse(response.body);
        cJSON *retry = body ? cJSON_GetObjectItemCaseSensitive(body, "retry_after") : nullptr;
        if (cJSON_IsNumber(retry) && std::isfinite(retry->valuedouble) && retry->valuedouble > response.retry)
          response.retry = retry->valuedouble;
        cJSON_Delete(body);
      }
      if (response.remaining == 0 && response.reset > response.retry) response.retry = response.reset;
      result.waitMs = thermo::secondsToMs(response.retry);
    }
    // Never echo the URL, payload, response body, SSID or password into logs.
    std::memset(job.url, 0, sizeof(job.url)); std::memset(job.payload, 0, sizeof(job.payload)); job.generation = 0;
    xQueueSend(results, &result, portMAX_DELAY);
  }
}
} // namespace

bool startWebhookWorker() {
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_NONE);
  jobs = xQueueCreate(1, sizeof(WebhookJob));
  results = xQueueCreate(1, sizeof(WebhookResult));
  if (!jobs || !results) return false;
  return xTaskCreate(worker, "discord", 8192, nullptr, 1, nullptr) == pdPASS;
}
bool submitWebhook(const WebhookJob &job) { return jobs && xQueueSend(jobs, &job, 0) == pdTRUE; }
bool receiveWebhookResult(WebhookResult &result) { return results && xQueueReceive(results, &result, 0) == pdTRUE; }
