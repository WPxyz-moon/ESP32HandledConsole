#include "fw_role.h"

#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#if CONFIG_FW_ROLE_FACTORY
#define FW_ROLE_COMPILED FW_ROLE_FACTORY
#elif CONFIG_FW_ROLE_GAME
#define FW_ROLE_COMPILED FW_ROLE_GAME
#elif CONFIG_FW_ROLE_AI
#define FW_ROLE_COMPILED FW_ROLE_AI
#else
#error "No firmware role selected in sdkconfig"
#endif

static const char *TAG = "fw_role";

fw_role_t fw_role_compiled(void)
{
    return FW_ROLE_COMPILED;
}

fw_role_t fw_role_running(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return FW_ROLE_UNKNOWN;
    }

    if (strcmp(running->label, "factory") == 0) {
        return FW_ROLE_FACTORY;
    }
    if (strcmp(running->label, "ota_0") == 0) {
        return FW_ROLE_GAME;
    }
    if (strcmp(running->label, "ota_1") == 0) {
        return FW_ROLE_AI;
    }

    return FW_ROLE_UNKNOWN;
}

const char *fw_role_name(fw_role_t role)
{
    switch (role) {
    case FW_ROLE_FACTORY:
        return "factory";
    case FW_ROLE_GAME:
        return "game";
    case FW_ROLE_AI:
        return "ai";
    default:
        return "unknown";
    }
}

bool fw_role_check(void)
{
    fw_role_t compiled = fw_role_compiled();
    fw_role_t running = fw_role_running();

    if (compiled == running) {
        return true;
    }

    ESP_LOGE(TAG, "firmware role mismatch: compiled=%s running=%s",
             fw_role_name(compiled), fw_role_name(running));
    return false;
}
