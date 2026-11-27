#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Must be called first in app_main: backlight off, LED off. */
esp_err_t bsp_board_early_init(void);

/* Full board bring-up in the order documented in docs/06:
 * early GPIO, NVS, display, input, IMU, audio, storage, power.
 * Backlight is left off; the application turns it on after the first frame.
 * Input is initialized without a callback; call bsp_input_set_callback()
 * afterwards if the application wants key events. */
esp_err_t bsp_board_init(void);

const char *bsp_board_name(void);

#ifdef __cplusplus
}
#endif
