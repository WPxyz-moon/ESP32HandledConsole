#include "bsp_input.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define BSP_INPUT_QUEUE_LEN 32
#define BSP_INPUT_TASK_STACK 3072
#define BSP_INPUT_TASK_PRIO  6
#define BSP_INPUT_POLL_MS    5

typedef struct {
    uint8_t key;
    uint8_t level;  /* raw GPIO level, 0 = pressed */
    TickType_t tick;
} input_edge_t;

static const char *TAG = "bsp_input";

static const gpio_num_t s_pins[BSP_KEY_COUNT] = {
    BSP_KEY_GPIO_UP, BSP_KEY_GPIO_DOWN, BSP_KEY_GPIO_LEFT,
    BSP_KEY_GPIO_RIGHT, BSP_KEY_GPIO_A, BSP_KEY_GPIO_B,
    BSP_KEY_GPIO_START, BSP_KEY_GPIO_CTRL,
};

static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static bsp_key_cb_t s_cb;
static void *s_cb_ctx;
static uint32_t s_stable_mask;
static uint32_t s_candidate_mask;
static TickType_t s_candidate_tick[BSP_KEY_COUNT];
static TickType_t s_press_tick[BSP_KEY_COUNT];
static bool s_long_sent[BSP_KEY_COUNT];
static bool s_suppress_repeat[BSP_KEY_COUNT];
static TickType_t s_last_repeat_tick[BSP_KEY_COUNT];
static uint32_t s_long_press_ms = BSP_KEY_LONG_PRESS_MS;
static bool s_inited;
static bool s_isr_service_owned;
static volatile uint32_t s_isr_count;
static volatile uint32_t s_drop_count;
static uint32_t s_event_count;

static bool pressed_at(bsp_key_id_t id)
{
    return gpio_get_level(s_pins[id]) == 0;
}

static void emit(bsp_key_id_t id, bsp_key_evt_t evt, TickType_t held)
{
    bsp_key_event_t event = {
        .id = id,
        .evt = evt,
        .held_ms = (uint32_t)held * portTICK_PERIOD_MS,
    };
    s_event_count++;
    if (s_cb != NULL) {
        s_cb(&event, s_cb_ctx);
    }
}

static void IRAM_ATTR key_isr(void *arg)
{
    input_edge_t edge = {
        .key = (uint8_t)(uintptr_t)arg,
        .tick = xTaskGetTickCountFromISR(),
    };
    edge.level = (uint8_t)gpio_get_level(s_pins[edge.key]);
    s_isr_count++;
    if (xQueueSendFromISR(s_queue, &edge, NULL) != pdTRUE) {
        s_drop_count++;
    }
}

static void input_task(void *arg)
{
    (void)arg;

    const TickType_t debounce_ticks = pdMS_TO_TICKS(BSP_KEY_DEBOUNCE_MS);
    input_edge_t edge;

    while (true) {
        if (xQueueReceive(s_queue, &edge, pdMS_TO_TICKS(BSP_INPUT_POLL_MS)) == pdTRUE) {
            uint32_t bit = 1U << edge.key;
            if (edge.level == 0) {
                s_candidate_mask |= bit;
            } else {
                s_candidate_mask &= ~bit;
            }
            s_candidate_tick[edge.key] = edge.tick;
        }

        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < BSP_KEY_COUNT; i++) {
            uint32_t bit = 1U << i;
            bool candidate = (s_candidate_mask & bit) != 0;
            bool stable = (s_stable_mask & bit) != 0;

            if (candidate == stable ||
                (now - s_candidate_tick[i]) < debounce_ticks) {
                continue;
            }

            bool actual = pressed_at((bsp_key_id_t)i);
            if (actual != candidate) {
                /* Still bouncing: follow the latest level. */
                s_candidate_mask = actual
                    ? (s_candidate_mask | bit)
                    : (s_candidate_mask & ~bit);
                s_candidate_tick[i] = now;
                continue;
            }

            if (candidate) {
                s_stable_mask |= bit;
                s_press_tick[i] = now;
                s_long_sent[i] = false;
                s_suppress_repeat[i] = false;
                s_last_repeat_tick[i] = now;
                emit((bsp_key_id_t)i, BSP_KEY_EVT_DOWN, 0);
            } else {
                TickType_t held = now - s_press_tick[i];
                s_stable_mask &= ~bit;
                s_long_sent[i] = false;
                s_suppress_repeat[i] = false;
                emit((bsp_key_id_t)i, BSP_KEY_EVT_UP, held);
            }
        }

        for (int i = 0; i < BSP_KEY_COUNT; i++) {
            uint32_t bit = 1U << i;
            if ((s_stable_mask & bit) == 0) {
                continue;
            }

            TickType_t held = now - s_press_tick[i];
            if (held < pdMS_TO_TICKS(s_long_press_ms)) {
                continue;
            }

            if (!s_long_sent[i]) {
                s_long_sent[i] = true;
                s_last_repeat_tick[i] = now;
                emit((bsp_key_id_t)i, BSP_KEY_EVT_LONG, held);
            } else if (!s_suppress_repeat[i] &&
                       (now - s_last_repeat_tick[i]) >=
                           pdMS_TO_TICKS(BSP_KEY_REPEAT_MS)) {
                s_last_repeat_tick[i] = now;
                emit((bsp_key_id_t)i, BSP_KEY_EVT_REPEAT, held);
            }
        }
    }
}

esp_err_t bsp_input_init(bsp_key_cb_t cb, void *ctx)
{
    if (s_inited) {
        return ESP_OK;
    }

    s_cb = cb;
    s_cb_ctx = ctx;

    gpio_config_t cfg = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        cfg.pin_bit_mask |= 1ULL << s_pins[i];
    }
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "key gpio config");

    s_queue = xQueueCreate(BSP_INPUT_QUEUE_LEN, sizeof(input_edge_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_OK) {
        s_isr_service_owned = true;
    } else if (err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "install isr service failed: %s", esp_err_to_name(err));
        vQueueDelete(s_queue);
        s_queue = NULL;
        return err;
    }

    TickType_t now = xTaskGetTickCount();
    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        s_candidate_tick[i] = now;
        s_press_tick[i] = now;
        s_long_sent[i] = false;
        s_suppress_repeat[i] = false;
        s_last_repeat_tick[i] = now;
        if (pressed_at((bsp_key_id_t)i)) {
            s_stable_mask |= 1U << i;
            s_candidate_mask |= 1U << i;
            s_long_sent[i] = true;  /* don't fire LONG for a key held at boot */
            s_suppress_repeat[i] = true;
        }
        ESP_RETURN_ON_ERROR(
            gpio_isr_handler_add(s_pins[i], key_isr, (void *)(uintptr_t)i),
            TAG, "add isr key %d", i);
    }

    if (xTaskCreate(input_task, "bsp_input", BSP_INPUT_TASK_STACK, NULL,
                    BSP_INPUT_TASK_PRIO, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG, "%d keys ready", BSP_KEY_COUNT);
    return ESP_OK;
}

esp_err_t bsp_input_set_callback(bsp_key_cb_t cb, void *ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
    return ESP_OK;
}

esp_err_t bsp_input_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        gpio_isr_handler_remove(s_pins[i]);
        gpio_reset_pin(s_pins[i]);
    }
    if (s_isr_service_owned) {
        gpio_uninstall_isr_service();
        s_isr_service_owned = false;
    }

    if (s_task != NULL) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    if (s_queue != NULL) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }

    s_stable_mask = 0;
    s_candidate_mask = 0;
    s_inited = false;
    return ESP_OK;
}

uint32_t bsp_input_state(void)
{
    return s_stable_mask;
}

bool bsp_input_pressed(bsp_key_id_t id)
{
    if (id >= BSP_KEY_COUNT) {
        return false;
    }
    return (s_stable_mask & (1U << id)) != 0;
}

esp_err_t bsp_input_set_long_press_ms(uint32_t ms)
{
    if (ms < 100) {
        return ESP_ERR_INVALID_ARG;
    }
    s_long_press_ms = ms;
    return ESP_OK;
}

esp_err_t bsp_input_get_stats(bsp_input_stats_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    out->isr_count = s_isr_count;
    out->drop_count = s_drop_count;
    out->event_count = s_event_count;
    return ESP_OK;
}

int bsp_input_key_gpio(bsp_key_id_t id)
{
    if (id >= BSP_KEY_COUNT) {
        return -1;
    }
    return (int)s_pins[id];
}
