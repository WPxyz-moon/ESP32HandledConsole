#include "event_bus.h"

#include "esp_log.h"
#include "freertos/queue.h"

#define EVENT_QUEUE_LEN 16

static const char *TAG = "event_bus";
static QueueHandle_t s_queue;

esp_err_t event_bus_init(void)
{
    if (s_queue != NULL) {
        return ESP_OK;
    }

    s_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(app_event_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "queue create failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "queue ready (%d events)", EVENT_QUEUE_LEN);
    return ESP_OK;
}

esp_err_t event_bus_post(const app_event_t *ev, TickType_t ticks_to_wait)
{
    if (s_queue == NULL || ev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return xQueueSend(s_queue, ev, ticks_to_wait) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

esp_err_t event_bus_post_from_isr(const app_event_t *ev)
{
    if (s_queue == NULL || ev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t woken = pdFALSE;
    BaseType_t ok = xQueueSendFromISR(s_queue, ev, &woken);
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }

    return ok == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t event_bus_receive(app_event_t *ev, TickType_t ticks_to_wait)
{
    if (s_queue == NULL || ev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return xQueueReceive(s_queue, ev, ticks_to_wait) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}
