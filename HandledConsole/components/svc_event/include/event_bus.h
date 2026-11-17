#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVT_SYS_BOOT = 0,
    EVT_KEY,
    EVT_IMU,
    EVT_NET,
    EVT_AUDIO,
    EVT_APP,
} event_type_t;

typedef struct {
    event_type_t type;
    union {
        int key_id;
        int code;
    } u;
} app_event_t;

esp_err_t event_bus_init(void);
esp_err_t event_bus_post(const app_event_t *ev, TickType_t ticks_to_wait);
esp_err_t event_bus_post_from_isr(const app_event_t *ev);
esp_err_t event_bus_receive(app_event_t *ev, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif
