#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*system_prepare_hook_t)(void);

/*
 * Register a hook that runs before switching firmware.
 * Modules that own audio, network, radio or filesystems register here.
 * Hooks are called in registration order. Keep them short and non-blocking.
 */
esp_err_t system_register_prepare_hook(system_prepare_hook_t hook);

/*
 * Run all registered prepare hooks.
 * Returns the first error, but still runs the remaining hooks.
 */
esp_err_t system_prepare_switch(void);

#ifdef __cplusplus
}
#endif
