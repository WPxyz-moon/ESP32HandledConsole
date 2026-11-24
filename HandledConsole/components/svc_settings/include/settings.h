#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_SCHEMA_VERSION 1u
#define SETTINGS_NAMESPACE      "hcfg"
#define SETTINGS_KEY_SCHEMA     "sys.schema_ver"

/*
 * Open the shared settings namespace.
 *
 * NVS must already be initialized by the BSP (bsp_storage_nvs_init()).
 * This component intentionally does not call nvs_flash_init().
 */
esp_err_t settings_init(void);
esp_err_t settings_deinit(void);

esp_err_t settings_get_u8(const char *key, uint8_t *out);
esp_err_t settings_set_u8(const char *key, uint8_t value);
esp_err_t settings_get_u16(const char *key, uint16_t *out);
esp_err_t settings_set_u16(const char *key, uint16_t value);
esp_err_t settings_get_i32(const char *key, int32_t *out);
esp_err_t settings_set_i32(const char *key, int32_t value);
esp_err_t settings_get_str(const char *key, char *buf, size_t *len);
esp_err_t settings_set_str(const char *key, const char *value);
esp_err_t settings_get_blob(const char *key, void *buf, size_t *len);
esp_err_t settings_set_blob(const char *key, const void *data, size_t len);

esp_err_t settings_erase_all(void);

#ifdef __cplusplus
}
#endif
