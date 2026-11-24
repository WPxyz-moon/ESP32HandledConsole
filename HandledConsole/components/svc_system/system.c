#include "system.h"

#include <stddef.h>

#include "esp_log.h"

#define SYSTEM_PREPARE_HOOK_MAX 4

static const char *TAG = "system";
static system_prepare_hook_t s_hooks[SYSTEM_PREPARE_HOOK_MAX];
static size_t s_hook_count;

esp_err_t system_register_prepare_hook(system_prepare_hook_t hook)
{
    if (hook == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_hook_count >= SYSTEM_PREPARE_HOOK_MAX) {
        return ESP_ERR_NO_MEM;
    }

    s_hooks[s_hook_count++] = hook;
    return ESP_OK;
}

esp_err_t system_prepare_switch(void)
{
    esp_err_t first_error = ESP_OK;

    for (size_t i = 0; i < s_hook_count; ++i) {
        esp_err_t err = s_hooks[i]();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "prepare hook %u failed: %s",
                     (unsigned)i, esp_err_to_name(err));
            if (first_error == ESP_OK) {
                first_error = err;
            }
        }
    }

    return first_error;
}
