#include "app_ops.h"

#include "esp_log.h"

static const char *TAG = "app_manager";
static const app_ops_t *s_current;
static app_t s_app;

static esp_err_t app_enter(const app_ops_t *ops, const void *arg)
{
    if (ops == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ops->init != NULL) {
        esp_err_t err = ops->init(&s_app);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "init %s failed: %s", ops->name, esp_err_to_name(err));
            return err;
        }
    }

    if (ops->enter != NULL) {
        esp_err_t err = ops->enter(&s_app, arg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "enter %s failed: %s", ops->name, esp_err_to_name(err));
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t app_manager_start(const app_ops_t *first)
{
    if (first == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_app.ops = first;
    s_app.ctx = NULL;
    s_current = first;

    esp_err_t err = app_enter(first, NULL);
    if (err != ESP_OK) {
        s_current = NULL;
        return err;
    }

    ESP_LOGI(TAG, "started app: %s", first->name != NULL ? first->name : "?");
    return ESP_OK;
}

esp_err_t app_manager_switch(const app_ops_t *next, const void *arg)
{
    if (next == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_current != NULL && s_current->exit != NULL) {
        esp_err_t err = s_current->exit(&s_app);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "exit %s failed: %s",
                     s_current->name, esp_err_to_name(err));
        }
    }

    s_app.ops = next;
    s_app.ctx = NULL;
    s_current = next;

    esp_err_t err = app_enter(next, arg);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "switched app: %s", next->name != NULL ? next->name : "?");
    return ESP_OK;
}

const app_ops_t *app_manager_current(void)
{
    return s_current;
}

void app_manager_dispatch(const app_event_t *ev)
{
    if (s_current != NULL && s_current->event != NULL && ev != NULL) {
        s_current->event(&s_app, ev);
    }
}
