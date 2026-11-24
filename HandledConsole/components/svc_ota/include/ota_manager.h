#pragma once

#include "esp_err.h"
#include "fw_role.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *ota_manager_running_label(void);

/*
 * Set the next boot partition and restart.
 *
 * The caller is responsible for saving its own data and for calling
 * system_prepare_switch() before this function.
 *
 * Returns an error and does not restart if the target partition is missing
 * or its image is invalid.
 */
esp_err_t ota_manager_switch(fw_role_t target);

esp_err_t ota_manager_boot_factory(void);
esp_err_t ota_manager_mark_valid(void);

#ifdef __cplusplus
}
#endif
