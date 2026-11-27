#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sleep_count;
    uint32_t wake_count;
    uint32_t last_wake_cause;
} bsp_power_stats_t;

esp_err_t bsp_power_init(void);
esp_err_t bsp_power_set_led(bool on);

/* Enter Light Sleep; any key selected in key_mask wakes the system.
 * key_mask is a bitmask of bsp_key_id_t; 0 means all keys. */
esp_err_t bsp_power_sleep(uint32_t key_mask);

esp_err_t bsp_power_get_stats(bsp_power_stats_t *out);

#ifdef __cplusplus
}
#endif
