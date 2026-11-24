#include "bsp_display.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

#define BSP_DISPLAY_BOUNCE_SIZE (16 * 1024)
#define BSP_DISPLAY_DMA_TIMEOUT_MS 1000
#define BSP_DISPLAY_BACKLIGHT_CHANNEL LEDC_CHANNEL_0
#define BSP_DISPLAY_BACKLIGHT_TIMER LEDC_TIMER_0

static const char *TAG = "bsp_display";

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_dma_done;
static uint8_t *s_bounce[2];
static size_t s_bounce_size;
static unsigned s_bounce_index;
static bool s_inited;
static bool s_backlight_inited;
static bool s_spi_bus_inited;
static bsp_display_stats_t s_stats;

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                          esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;

    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_dma_done, &high_task_woken);
    return high_task_woken == pdTRUE;
}

static esp_err_t backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BSP_DISPLAY_BACKLIGHT_TIMER,
        .freq_hz = BSP_LCD_BACKLIGHT_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");

    ledc_channel_config_t channel = {
        .gpio_num = BSP_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BSP_DISPLAY_BACKLIGHT_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BSP_DISPLAY_BACKLIGHT_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    return ledc_channel_config(&channel);
}

static esp_err_t panel_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = BSP_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BSP_LCD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BSP_DISPLAY_BOUNCE_SIZE,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "LCD SPI bus");
    s_spi_bus_inited = true;

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BSP_LCD_CS,
        .dc_gpio_num = BSP_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,
                                 &io_config, &s_io),
        TAG, "panel IO");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = BSP_LCD_RGB_ORDER_BGR
            ? LCD_RGB_ELEMENT_ORDER_BGR
            : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io, &panel_config, &s_panel),
                        TAG, "ST7789 panel");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");

#if BSP_LCD_INVERT_COLOR
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "invert");
#endif
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, BSP_LCD_SWAP_XY),
                        TAG, "swap xy");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, BSP_LCD_MIRROR_X,
                                             BSP_LCD_MIRROR_Y),
                        TAG, "mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, 0, BSP_LCD_Y_OFFSET),
                        TAG, "y gap");
    return esp_lcd_panel_disp_on_off(s_panel, true);
}

esp_err_t bsp_display_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight init");
    s_backlight_inited = true;

    s_dma_done = xSemaphoreCreateBinary();
    if (s_dma_done == NULL) {
        bsp_display_deinit();
        return ESP_ERR_NO_MEM;
    }

    s_bounce_size = BSP_DISPLAY_BOUNCE_SIZE;
    s_bounce[0] = heap_caps_malloc(s_bounce_size,
                                   MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_bounce[1] = heap_caps_malloc(s_bounce_size,
                                   MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (s_bounce[0] == NULL || s_bounce[1] == NULL) {
        ESP_LOGE(TAG, "bounce buffer alloc failed");
        bsp_display_deinit();
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = panel_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel init failed: %s", esp_err_to_name(err));
        bsp_display_deinit();
        return err;
    }

    s_inited = true;
    ESP_LOGI(TAG, "ST7789 %dx%d ready", BSP_DISPLAY_WIDTH, BSP_DISPLAY_HEIGHT);
    return ESP_OK;
}

esp_err_t bsp_display_deinit(void)
{
    if (!s_inited && !s_backlight_inited && !s_spi_bus_inited) {
        return ESP_OK;
    }

    if (s_panel != NULL) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    if (s_io != NULL) {
        esp_lcd_panel_io_del(s_io);
        s_io = NULL;
    }
    if (s_spi_bus_inited) {
        spi_bus_free(SPI2_HOST);
        s_spi_bus_inited = false;
    }

    if (s_backlight_inited) {
        ledc_stop(LEDC_LOW_SPEED_MODE, BSP_DISPLAY_BACKLIGHT_CHANNEL, 0);
        gpio_config_t bl = {
            .pin_bit_mask = 1ULL << BSP_LCD_BL,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&bl);
        gpio_set_level((gpio_num_t)BSP_LCD_BL, 0);
        s_backlight_inited = false;
    }

    for (int i = 0; i < 2; i++) {
        heap_caps_free(s_bounce[i]);
        s_bounce[i] = NULL;
    }
    if (s_dma_done != NULL) {
        vSemaphoreDelete(s_dma_done);
        s_dma_done = NULL;
    }

    s_inited = false;
    return ESP_OK;
}

static void copy_swap(uint8_t *dst, const uint8_t *src, size_t bytes)
{
#if BSP_DISPLAY_SWAP_BYTES
    for (size_t i = 0; i + 1 < bytes; i += 2) {
        dst[i] = src[i + 1];
        dst[i + 1] = src[i];
    }
#else
    memcpy(dst, src, bytes);
#endif
}

esp_err_t bsp_display_flush(int x_start, int y_start, int x_end, int y_end,
                            const void *rgb565)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (rgb565 == NULL || x_start < 0 || y_start < 0 ||
        x_end > BSP_DISPLAY_WIDTH || y_end > BSP_DISPLAY_HEIGHT ||
        x_end <= x_start || y_end <= y_start) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *src = (const uint8_t *)rgb565;
    const int width = x_end - x_start;
    const int height = y_end - y_start;
    const size_t row_bytes = (size_t)width * sizeof(uint16_t);
    int rows_per_chunk = (int)(s_bounce_size / row_bytes);
    if (rows_per_chunk < 1) {
        return ESP_ERR_INVALID_SIZE;
    }

    int64_t start_us = esp_timer_get_time();
    for (int row = 0; row < height; row += rows_per_chunk) {
        int rows = height - row;
        if (rows > rows_per_chunk) {
            rows = rows_per_chunk;
        }
        size_t bytes = (size_t)rows * row_bytes;
        uint8_t *bounce = s_bounce[s_bounce_index];

        copy_swap(bounce, src + (size_t)row * row_bytes, bytes);

        /* Drop a stale completion from an earlier timed-out transfer. */
        xSemaphoreTake(s_dma_done, 0);

        esp_err_t err = esp_lcd_panel_draw_bitmap(
            s_panel, x_start, y_start + row, x_end, y_start + row + rows,
            bounce);
        if (err != ESP_OK) {
            s_stats.flush_timeout++;
            return err;
        }

        if (xSemaphoreTake(s_dma_done,
                           pdMS_TO_TICKS(BSP_DISPLAY_DMA_TIMEOUT_MS)) != pdTRUE) {
            s_stats.flush_timeout++;
            ESP_LOGW(TAG, "DMA timeout");
            return ESP_ERR_TIMEOUT;
        }

        s_bounce_index ^= 1;
    }

    s_stats.flush_count++;
    s_stats.last_flush_us = (uint32_t)(esp_timer_get_time() - start_us);
    return ESP_OK;
}

esp_err_t bsp_display_draw(int x, int y, int w, int h, const void *rgb565)
{
    if (w <= 0 || h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_display_flush(x, y, x + w, y + h, rgb565);
}

esp_err_t bsp_display_wait_idle(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return s_inited ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t bsp_display_set_backlight(uint8_t percent)
{
    if (!s_backlight_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (percent > 100) {
        percent = 100;
    }

    uint32_t duty = ((uint32_t)percent * 1023U) / 100U;
    ESP_RETURN_ON_ERROR(
        ledc_set_duty(LEDC_LOW_SPEED_MODE, BSP_DISPLAY_BACKLIGHT_CHANNEL, duty),
        TAG, "set duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE,
                            BSP_DISPLAY_BACKLIGHT_CHANNEL);
}

esp_err_t bsp_display_get_stats(bsp_display_stats_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = s_stats;
    return ESP_OK;
}

int bsp_display_width(void)
{
    return BSP_DISPLAY_WIDTH;
}

int bsp_display_height(void)
{
    return BSP_DISPLAY_HEIGHT;
}
