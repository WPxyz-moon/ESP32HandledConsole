#include "ota_manager.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

static const char *TAG = "ota_manager";

const char *ota_manager_running_label(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running != NULL ? running->label : "unknown";
}

esp_err_t ota_manager_boot(esp_partition_subtype_t subtype)
{
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, subtype, NULL);

    if (part == NULL) {
        ESP_LOGE(TAG, "app partition subtype %d not found", subtype);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set boot partition %s failed: %s",
                 part->label, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "boot partition set to %s, restarting", part->label);
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_manager_boot_factory(void)
{
    return ota_manager_boot(ESP_PARTITION_SUBTYPE_APP_FACTORY);
}

esp_err_t ota_manager_mark_valid(void)
{
    return esp_ota_mark_app_valid_cancel_rollback();
}
