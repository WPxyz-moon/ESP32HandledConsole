#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NVS is shared by all three firmwares. Call once before using settings. */
esp_err_t bsp_storage_nvs_init(void);

/* NVS + assets (non-fatal) + SD (non-fatal, 3 retries). */
esp_err_t bsp_storage_init(void);

esp_err_t bsp_storage_sd_mount(void);
esp_err_t bsp_storage_sd_unmount(void);
bool bsp_storage_sd_ready(void);

esp_err_t bsp_storage_assets_mount(void);

const char *bsp_storage_sd_path(void);
const char *bsp_storage_assets_path(void);

#ifdef __cplusplus
}
#endif
