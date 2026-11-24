#include "bsp_board.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "bsp_board";

static esp_err_t configure_output(int pin, int level)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "configure GPIO%d failed", pin);
    return gpio_set_level((gpio_num_t)pin, level);
}

esp_err_t bsp_board_early_init(void)
{
    /* Backlight is NPN low-side and has a 10k pull-up: drive it low ASAP. */
    ESP_RETURN_ON_ERROR(configure_output(BSP_LCD_BL, 0), TAG, "backlight off");
    /* User LED is active low. */
    ESP_RETURN_ON_ERROR(configure_output(BSP_LED, 1), TAG, "led off");
    return ESP_OK;
}

esp_err_t bsp_board_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_board_early_init(), TAG, "early init failed");
    ESP_LOGI(TAG, "board: %s", BSP_BOARD_NAME);
    return ESP_OK;
}

const char *bsp_board_name(void)
{
    return BSP_BOARD_NAME;
}
