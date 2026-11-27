#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_DISPLAY_WIDTH  240
#define BSP_DISPLAY_HEIGHT 280

typedef struct {
    uint32_t flush_count;
    uint32_t flush_timeout;
    uint32_t last_flush_us;
} bsp_display_stats_t;

/*
 * All pixel buffers are little-endian RGB565 (as produced by LVGL without
 * LV_COLOR_16_SWAP and by most NES palettes). The driver performs the
 * byte swap required by ST7789 while copying into its DMA bounce buffer.
 */
esp_err_t bsp_display_init(void);
esp_err_t bsp_display_deinit(void);

/* x_end / y_end are exclusive. Blocks until the DMA transfer completes.
 * Source rows must be tightly packed (stride = width * sizeof(uint16_t)). */
esp_err_t bsp_display_flush(int x_start, int y_start, int x_end, int y_end,
                            const void *rgb565);
esp_err_t bsp_display_draw(int x, int y, int w, int h, const void *rgb565);

/* Flush is synchronous; kept for API symmetry and future double buffering. */
esp_err_t bsp_display_wait_idle(uint32_t timeout_ms);

/* 0 = off, 100 = maximum. */
esp_err_t bsp_display_set_backlight(uint8_t percent);

esp_err_t bsp_display_get_stats(bsp_display_stats_t *out);

int bsp_display_width(void);
int bsp_display_height(void);

#ifdef __cplusplus
}
#endif
