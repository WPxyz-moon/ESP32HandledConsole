#include "event_bus.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define EVENT_QUEUE_LEN 32
#define EVENT_SUB_MAX   16
#define EVENT_TASK_STACK 4096
#define EVENT_TASK_PRIO  5

typedef struct {
    event_type_t type;
    event_handler_t handler;
    void *ctx;
} subscriber_t;

static const char *TAG = "event_bus";
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;
static subscriber_t s_subscribers[EVENT_SUB_MAX];
static size_t s_subscriber_count;
static event_bus_stats_t s_stats;

static void event_task(void *arg)
{
    (void)arg;
    app_event_t ev;

    for (;;) {
        if (xQueueReceive(s_queue, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        subscriber_t local[EVENT_SUB_MAX];
        size_t count = 0;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        count = s_subscriber_count;
        for (size_t i = 0; i < count; ++i) {
            local[i] = s_subscribers[i];
        }
        xSemaphoreGive(s_lock);

        for (size_t i = 0; i < count; ++i) {
            if (local[i].type == ev.type && local[i].handler != NULL) {
                local[i].handler(&ev, local[i].ctx);
            }
        }

        __atomic_fetch_add(&s_stats.dispatched, 1, __ATOMIC_RELAXED);
    }
}

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

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        ESP_LOGE(TAG, "mutex create failed");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(event_task, "event_task", EVENT_TASK_STACK,
                                NULL, EVENT_TASK_PRIO, NULL);
    if (ok != pdPASS) {
        vSemaphoreDelete(s_lock);
        vQueueDelete(s_queue);
        s_lock = NULL;
        s_queue = NULL;
        ESP_LOGE(TAG, "task create failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "queue ready (%d events)", EVENT_QUEUE_LEN);
    return ESP_OK;
}

esp_err_t event_bus_subscribe(event_type_t type,
                              event_handler_t handler, void *ctx)
{
    if (s_lock == NULL || handler == NULL || type < 0 || type >= EVT_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_ERR_NO_MEM;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_subscriber_count < EVENT_SUB_MAX) {
        s_subscribers[s_subscriber_count].type = type;
        s_subscribers[s_subscriber_count].handler = handler;
        s_subscribers[s_subscriber_count].ctx = ctx;
        s_subscriber_count++;
        err = ESP_OK;
    }
    xSemaphoreGive(s_lock);

    return err;
}

esp_err_t event_bus_post(const app_event_t *ev, TickType_t ticks_to_wait)
{
    if (s_queue == NULL || ev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    app_event_t copy = *ev;
    if (copy.timestamp_ms == 0) {
        copy.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    }

    if (xQueueSend(s_queue, &copy, ticks_to_wait) != pdTRUE) {
        __atomic_fetch_add(&s_stats.dropped, 1, __ATOMIC_RELAXED);
        return ESP_ERR_TIMEOUT;
    }

    __atomic_fetch_add(&s_stats.posted, 1, __ATOMIC_RELAXED);
    return ESP_OK;
}

esp_err_t event_bus_post_from_isr(const app_event_t *ev)
{
    if (s_queue == NULL || ev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    app_event_t copy = *ev;
    if (copy.timestamp_ms == 0) {
        copy.timestamp_ms = (uint32_t)(xTaskGetTickCountFromISR() *
                                       portTICK_PERIOD_MS);
    }

    BaseType_t woken = pdFALSE;
    BaseType_t ok = xQueueSendFromISR(s_queue, &copy, &woken);
    if (ok != pdTRUE) {
        __atomic_fetch_add(&s_stats.dropped, 1, __ATOMIC_RELAXED);
        return ESP_ERR_TIMEOUT;
    }

    __atomic_fetch_add(&s_stats.posted, 1, __ATOMIC_RELAXED);
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    return ESP_OK;
}

void event_bus_get_stats(event_bus_stats_t *out)
{
    if (out == NULL) {
        return;
    }

    out->posted = __atomic_load_n(&s_stats.posted, __ATOMIC_RELAXED);
    out->dropped = __atomic_load_n(&s_stats.dropped, __ATOMIC_RELAXED);
    out->dispatched = __atomic_load_n(&s_stats.dispatched, __ATOMIC_RELAXED);
}
