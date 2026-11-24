#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EVT_SYS_BOOT = 0,
    EVT_SYS_POWER,
    EVT_KEY,
    EVT_IMU,
    EVT_NET,
    EVT_AUDIO,
    EVT_APP,
    EVT_TYPE_MAX,
} event_type_t;

typedef struct {
    event_type_t type;
    uint32_t timestamp_ms; /* monotonic milliseconds since boot */

    union {
        struct {
            uint8_t id;       /* bsp_key_id_t */
            uint8_t evt;      /* BSP_KEY_EVT_* */
            uint16_t state;   /* current key bitmap */
            uint32_t held_ms;
        } key;
        struct {
            uint8_t gesture;  /* gesture bitmask, not raw samples */
        } imu;
        struct {
            uint8_t state;
            int code;
        } net;
        struct {
            uint8_t state;
        } audio;
        struct {
            uint8_t reason;
        } sys;
        struct {
            int code;
        } app;
    } u;
} app_event_t;

typedef struct {
    uint32_t posted;
    uint32_t dropped;
    uint32_t dispatched;
} event_bus_stats_t;

typedef void (*event_handler_t)(const app_event_t *ev, void *ctx);

esp_err_t event_bus_init(void);
esp_err_t event_bus_subscribe(event_type_t type,
                              event_handler_t handler, void *ctx);
esp_err_t event_bus_post(const app_event_t *ev, TickType_t ticks_to_wait);
esp_err_t event_bus_post_from_isr(const app_event_t *ev);
void event_bus_get_stats(event_bus_stats_t *out);

#ifdef __cplusplus
}
#endif
