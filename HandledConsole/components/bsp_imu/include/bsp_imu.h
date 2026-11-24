#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
} bsp_imu_raw_t;

typedef struct {
    float ax_g, ay_g, az_g;
    float gx_dps, gy_dps, gz_dps;
    uint32_t timestamp_ms;
} bsp_imu_sample_t;

typedef struct {
    uint32_t samples;
    uint32_t read_errors;
    uint32_t init_errors;
} bsp_imu_stats_t;

typedef void (*bsp_imu_cb_t)(const bsp_imu_sample_t *sample, void *ctx);

esp_err_t bsp_imu_init(uint32_t sample_hz);
esp_err_t bsp_imu_deinit(void);

esp_err_t bsp_imu_read_latest(bsp_imu_sample_t *out);
esp_err_t bsp_imu_read_raw(bsp_imu_raw_t *out);

/* Callback runs in the IMU sampling task context. */
esp_err_t bsp_imu_set_callback(bsp_imu_cb_t cb, void *ctx);

esp_err_t bsp_imu_get_stats(bsp_imu_stats_t *out);

#ifdef __cplusplus
}
#endif
