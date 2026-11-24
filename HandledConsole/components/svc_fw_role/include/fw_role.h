#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FW_ROLE_UNKNOWN = -1,
    FW_ROLE_FACTORY = 0,
    FW_ROLE_GAME = 1,
    FW_ROLE_AI = 2,
} fw_role_t;

/* Role selected at compile time by the project's sdkconfig. */
fw_role_t fw_role_compiled(void);

/* Role derived from the partition the bootloader actually started. */
fw_role_t fw_role_running(void);

const char *fw_role_name(fw_role_t role);

/* Log an error and return false if compiled and running roles differ. */
bool fw_role_check(void);

#ifdef __cplusplus
}
#endif
