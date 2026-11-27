#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_KEY_UP = 0,
    BSP_KEY_DOWN,
    BSP_KEY_LEFT,
    BSP_KEY_RIGHT,
    BSP_KEY_A,
    BSP_KEY_B,
    BSP_KEY_START,
    BSP_KEY_CTRL,
    BSP_KEY_COUNT
} bsp_key_id_t;

typedef enum {
    BSP_KEY_EVT_DOWN = 0,
    BSP_KEY_EVT_UP,
    BSP_KEY_EVT_LONG,
    BSP_KEY_EVT_REPEAT
} bsp_key_evt_t;

typedef struct {
    bsp_key_id_t id;
    bsp_key_evt_t evt;
    uint32_t held_ms;
} bsp_key_event_t;

typedef struct {
    uint32_t isr_count;
    uint32_t drop_count;
    uint32_t event_count;
} bsp_input_stats_t;

/* Callback runs in the input task context, never in the ISR. */
typedef void (*bsp_key_cb_t)(const bsp_key_event_t *ev, void *ctx);

esp_err_t bsp_input_init(bsp_key_cb_t cb, void *ctx);
esp_err_t bsp_input_deinit(void);

/* Change the callback after init, e.g. after bsp_board_init(). */
esp_err_t bsp_input_set_callback(bsp_key_cb_t cb, void *ctx);

/* Bit n set means bsp_key_id_t n is currently pressed. */
uint32_t bsp_input_state(void);
bool bsp_input_pressed(bsp_key_id_t id);

esp_err_t bsp_input_set_long_press_ms(uint32_t ms);
esp_err_t bsp_input_get_stats(bsp_input_stats_t *out);

/* Exposed for bsp_power's GPIO wake-up configuration. */
int bsp_input_key_gpio(bsp_key_id_t id);

#ifdef __cplusplus
}
#endif
