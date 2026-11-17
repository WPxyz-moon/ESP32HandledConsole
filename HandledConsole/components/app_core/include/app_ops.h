#pragma once

#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct app app_t;

typedef struct {
    const char *name;
    esp_err_t (*init)(app_t *app);
    esp_err_t (*enter)(app_t *app, const void *arg);
    void (*event)(app_t *app, const app_event_t *ev);
    esp_err_t (*exit)(app_t *app);
} app_ops_t;

struct app {
    const app_ops_t *ops;
    void *ctx;
};

esp_err_t app_manager_start(const app_ops_t *first);
esp_err_t app_manager_switch(const app_ops_t *next, const void *arg);
const app_ops_t *app_manager_current(void);
void app_manager_dispatch(const app_event_t *ev);

#ifdef __cplusplus
}
#endif
