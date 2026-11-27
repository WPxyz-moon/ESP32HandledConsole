#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Must be called first in app_main: backlight off, LED off. */
esp_err_t bsp_board_early_init(void);

/* Board-level bring-up. Currently an alias for bsp_board_early_init(). */
esp_err_t bsp_board_init(void);

const char *bsp_board_name(void);

#ifdef __cplusplus
}
#endif
