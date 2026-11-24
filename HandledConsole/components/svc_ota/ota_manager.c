#include "ota_manager.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

static const char *TAG = "ota_manager";

const char *ota_manager_running_label(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running != NULL ? running->label : "unknown";
}

static esp_err_t target_to_subtype(fw_role_t target,
                                   esp_partition_subtype_t *subtype)
{
    switch (target) {
    case FW_ROLE_FACTORY:
        *subtype = ESP_PARTITION_SUBTYPE_APP_FACTORY;
        return ESP_OK;
    case FW_ROLE_GAME:
        *subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        return ESP_OK;
    case FW_ROLE_AI:
        *subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t ota_manager_switch(fw_role_t target)
{
    esp_partition_subtype_t subtype;
    esp_err_t err = target_to_subtype(target, &subtype);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "invalid target role: %s", fw_role_name(target));
        return err;
    }

    if (target == fw_role_running()) {
        ESP_LOGW(TAG, "target %s is already running", fw_role_name(target));
    }

    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, subtype, NULL);
    if (part == NULL) {
        ESP_LOGE(TAG, "app partition for %s not found", fw_role_name(target));
        return ESP_ERR_NOT_FOUND;
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set boot partition %s failed: %s",
                 part->label, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "boot partition set to %s (%s), restarting",
             part->label, fw_role_name(target));
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_manager_boot_factory(void)
{
    return ota_manager_switch(FW_ROLE_FACTORY);
}

esp_err_t ota_manager_mark_valid(void)
{
    return esp_ota_mark_app_valid_cancel_rollback();
}
