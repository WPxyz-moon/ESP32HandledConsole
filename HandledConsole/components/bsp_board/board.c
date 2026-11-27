#include "bsp_board.h"
#include "board_config.h"

#include "bsp_audio.h"
#include "bsp_display.h"
#include "bsp_imu.h"
#include "bsp_input.h"
#include "bsp_power.h"
#include "bsp_storage.h"

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
    ESP_RETURN_ON_ERROR(bsp_storage_nvs_init(), TAG, "NVS init");
    ESP_RETURN_ON_ERROR(bsp_display_init(), TAG, "display init");
    ESP_RETURN_ON_ERROR(bsp_input_init(NULL, NULL), TAG, "input init");
    ESP_RETURN_ON_ERROR(bsp_imu_init(100), TAG, "IMU init");
    ESP_RETURN_ON_ERROR(bsp_audio_init(), TAG, "audio init");
    ESP_RETURN_ON_ERROR(bsp_storage_init(), TAG, "storage init");
    ESP_RETURN_ON_ERROR(bsp_power_init(), TAG, "power init");

    ESP_LOGI(TAG, "board ready: %s (backlight off)", BSP_BOARD_NAME);
    return ESP_OK;
}

const char *bsp_board_name(void)
{
    return BSP_BOARD_NAME;
}
