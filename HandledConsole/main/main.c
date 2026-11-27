#include "app_ops.h"
#include "bsp_board.h"
#include "event_bus.h"
#include "fw_role.h"
#include "ota_manager.h"
#include "system.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG          "factory"
#define NEXT_TARGET  FW_ROLE_GAME
#define AUTO_SWITCH  1

static esp_err_t skeleton_init(app_t *app)
{
    (void)app;
    ESP_LOGI(TAG, "app init");
    return ESP_OK;
}

static esp_err_t skeleton_enter(app_t *app, const void *arg)
{
    (void)app;
    (void)arg;
    ESP_LOGI(TAG, "app enter");
    return ESP_OK;
}

static void skeleton_event(app_t *app, const app_event_t *ev)
{
    (void)app;
    ESP_LOGI(TAG, "app event type=%d", ev->type);
}

static esp_err_t skeleton_exit(app_t *app)
{
    (void)app;
    ESP_LOGI(TAG, "app exit");
    return ESP_OK;
}

static const app_ops_t skeleton_app = {
    .name = "skeleton",
    .init = skeleton_init,
    .enter = skeleton_enter,
    .event = skeleton_event,
    .exit = skeleton_exit,
};

static void on_boot_event(const app_event_t *ev, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "event bus ok, type=%d, ts=%u",
             ev->type, (unsigned)ev->timestamp_ms);
}

void app_main(void)
{
    /* Must be first: the backlight is pulled up by hardware until we drive it low. */
    ESP_ERROR_CHECK(bsp_board_early_init());

    ESP_LOGI(TAG, "==== %s boot ====", bsp_board_name());
    ESP_LOGI(TAG, "role compiled=%s running=%s",
             fw_role_name(fw_role_compiled()),
             fw_role_name(fw_role_running()));
    ESP_LOGI(TAG, "running partition: %s", ota_manager_running_label());
    ESP_LOGI(TAG, "reset reason: %d", (int)esp_reset_reason());
    ESP_LOGI(TAG, "PSRAM total: %u bytes",
             (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));

    if (!fw_role_check()) {
        ESP_LOGE(TAG, "continuing with compiled role");
    }

    ESP_ERROR_CHECK(event_bus_init());
    ESP_ERROR_CHECK(event_bus_subscribe(EVT_SYS_BOOT, on_boot_event, NULL));

    app_event_t event = { .type = EVT_SYS_BOOT };
    esp_err_t err = event_bus_post(&event, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "event post failed: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(app_manager_start(&skeleton_app));

#if AUTO_SWITCH
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "switching to %s", fw_role_name(NEXT_TARGET));
    err = system_prepare_switch();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "prepare switch failed: %s", esp_err_to_name(err));
    } else {
        err = ota_manager_switch(NEXT_TARGET);
        ESP_LOGE(TAG, "switch failed: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGI(TAG, "AUTO_SWITCH disabled, staying in this firmware");
#endif

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
