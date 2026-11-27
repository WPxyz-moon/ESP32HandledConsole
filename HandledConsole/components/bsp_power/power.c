#include "bsp_power.h"
#include "board_config.h"
#include "bsp_input.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "bsp_power";

static bool s_inited;
static bsp_power_stats_t s_stats;

esp_err_t bsp_power_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << BSP_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&led), TAG, "LED gpio");
    ESP_RETURN_ON_ERROR(gpio_set_level((gpio_num_t)BSP_LED, 1), TAG, "LED off");

    s_stats.last_wake_cause = (uint32_t)esp_sleep_get_wakeup_cause();
    s_inited = true;
    ESP_LOGI(TAG, "power ready (last wake cause %u)",
             (unsigned)s_stats.last_wake_cause);
    return ESP_OK;
}

esp_err_t bsp_power_set_led(bool on)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return gpio_set_level((gpio_num_t)BSP_LED, on ? 0 : 1);
}

esp_err_t bsp_power_sleep(uint32_t key_mask)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (key_mask == 0) {
        key_mask = (1U << BSP_KEY_COUNT) - 1U;
    }

    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        if ((key_mask & (1U << i)) == 0) {
            continue;
        }
        int pin = bsp_input_key_gpio((bsp_key_id_t)i);
        if (pin < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(
            gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL),
            TAG, "wake gpio %d", pin);
    }
    ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG, "gpio wakeup");

    s_stats.sleep_count++;
    esp_light_sleep_start();

    for (int i = 0; i < BSP_KEY_COUNT; i++) {
        if ((key_mask & (1U << i)) == 0) {
            continue;
        }
        int pin = bsp_input_key_gpio((bsp_key_id_t)i);
        if (pin >= 0) {
            gpio_wakeup_disable((gpio_num_t)pin);
        }
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);

    s_stats.wake_count++;
    s_stats.last_wake_cause = (uint32_t)esp_sleep_get_wakeup_cause();
    return ESP_OK;
}

esp_err_t bsp_power_get_stats(bsp_power_stats_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = s_stats;
    return ESP_OK;
}
