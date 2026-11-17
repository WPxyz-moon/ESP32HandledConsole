#pragma once

#include "esp_err.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *ota_manager_running_label(void);
esp_err_t ota_manager_boot(esp_partition_subtype_t subtype);
esp_err_t ota_manager_boot_factory(void);
esp_err_t ota_manager_mark_valid(void);

#ifdef __cplusplus
}
#endif
