#include "settings.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "settings";
static nvs_handle_t s_handle;
static SemaphoreHandle_t s_lock;
static bool s_ready;

static esp_err_t settings_lock(void)
{
    if (!s_ready || s_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

static void settings_unlock(void)
{
    xSemaphoreGive(s_lock);
}

esp_err_t settings_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s (is NVS initialized?)",
                 esp_err_to_name(err));
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
        return err;
    }

    s_ready = true;

    uint32_t schema = 0;
    err = nvs_get_u32(s_handle, SETTINGS_KEY_SCHEMA, &schema);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        schema = SETTINGS_SCHEMA_VERSION;
        err = nvs_set_u32(s_handle, SETTINGS_KEY_SCHEMA, schema);
        if (err == ESP_OK) {
            err = nvs_commit(s_handle);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "schema write failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "schema initialized: %u", (unsigned)schema);
        }
    } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "schema read failed: %s", esp_err_to_name(err));
    } else if (schema < SETTINGS_SCHEMA_VERSION) {
        ESP_LOGW(TAG, "schema migration %u -> %u",
                 (unsigned)schema, (unsigned)SETTINGS_SCHEMA_VERSION);
        /* Add migrations here. */
        err = nvs_set_u32(s_handle, SETTINGS_KEY_SCHEMA,
                          SETTINGS_SCHEMA_VERSION);
        if (err == ESP_OK) {
            err = nvs_commit(s_handle);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "schema migrate failed: %s", esp_err_to_name(err));
        }
    } else if (schema > SETTINGS_SCHEMA_VERSION) {
        ESP_LOGW(TAG, "settings schema %u is newer than firmware %u",
                 (unsigned)schema, (unsigned)SETTINGS_SCHEMA_VERSION);
    }

    ESP_LOGI(TAG, "ready");
    return ESP_OK;
}

esp_err_t settings_deinit(void)
{
    if (!s_ready) {
        return ESP_OK;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    nvs_close(s_handle);
    s_ready = false;
    xSemaphoreGive(s_lock);
    vSemaphoreDelete(s_lock);
    s_lock = NULL;
    return ESP_OK;
}

esp_err_t settings_get_u8(const char *key, uint8_t *out)
{
    if (key == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_u8(s_handle, key, out);
    settings_unlock();
    return err;
}

esp_err_t settings_set_u8(const char *key, uint8_t value)
{
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(s_handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    settings_unlock();
    return err;
}

esp_err_t settings_get_u16(const char *key, uint16_t *out)
{
    if (key == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_u16(s_handle, key, out);
    settings_unlock();
    return err;
}

esp_err_t settings_set_u16(const char *key, uint16_t value)
{
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u16(s_handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    settings_unlock();
    return err;
}

esp_err_t settings_get_i32(const char *key, int32_t *out)
{
    if (key == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_i32(s_handle, key, out);
    settings_unlock();
    return err;
}

esp_err_t settings_set_i32(const char *key, int32_t value)
{
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_i32(s_handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    settings_unlock();
    return err;
}

esp_err_t settings_get_str(const char *key, char *buf, size_t *len)
{
    if (key == NULL || buf == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_str(s_handle, key, buf, len);
    settings_unlock();
    return err;
}

esp_err_t settings_set_str(const char *key, const char *value)
{
    if (key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(s_handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    settings_unlock();
    return err;
}

esp_err_t settings_get_blob(const char *key, void *buf, size_t *len)
{
    if (key == NULL || buf == NULL || len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_blob(s_handle, key, buf, len);
    settings_unlock();
    return err;
}

esp_err_t settings_set_blob(const char *key, const void *data, size_t len)
{
    if (key == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(s_handle, key, data, len);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    settings_unlock();
    return err;
}

esp_err_t settings_erase_all(void)
{
    esp_err_t err = settings_lock();
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(s_handle);
    if (err == ESP_OK) {
        err = nvs_commit(s_handle);
    }
    settings_unlock();
    return err;
}
