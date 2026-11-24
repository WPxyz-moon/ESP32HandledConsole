#include "bsp_storage.h"
#include "board_config.h"

#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"

static const char *TAG = "bsp_storage";

static bool s_nvs_inited;
static bool s_sd_bus_inited;
static bool s_sd_mounted;
static bool s_assets_mounted;
static sdmmc_card_t *s_card;

esp_err_t bsp_storage_nvs_init(void)
{
    if (s_nvs_inited) {
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS init: %s, erasing", esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
        err = nvs_flash_init();
    }
    if (err == ESP_OK) {
        s_nvs_inited = true;
    }
    return err;
}

static esp_err_t sd_bus_init(void)
{
    if (s_sd_bus_inited) {
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = BSP_SD_MOSI,
        .miso_io_num = BSP_SD_MISO,
        .sclk_io_num = BSP_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "SD SPI bus");
    s_sd_bus_inited = true;
    return ESP_OK;
}

esp_err_t bsp_storage_sd_mount(void)
{
    if (s_sd_mounted) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(sd_bus_init(), TAG, "SD bus init");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    host.max_freq_khz = BSP_SD_SPI_FREQ_KHZ;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = SPI3_HOST;
    slot_config.gpio_cs = (gpio_num_t)BSP_SD_CS;

    esp_err_t err = esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, &host,
                                            &slot_config, &mount_config,
                                            &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(err));
        s_card = NULL;
        return err;
    }

    s_sd_mounted = true;
    ESP_LOGI(TAG, "SD mounted at %s", BSP_SD_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t bsp_storage_sd_unmount(void)
{
    if (!s_sd_mounted || s_card == NULL) {
        return ESP_OK;
    }

    esp_err_t err = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, s_card);
    s_card = NULL;
    s_sd_mounted = false;
    return err;
}

bool bsp_storage_sd_ready(void)
{
    return s_sd_mounted;
}

esp_err_t bsp_storage_assets_mount(void)
{
    if (s_assets_mounted) {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = BSP_ASSETS_MOUNT_POINT,
        .partition_label = BSP_ASSETS_PARTITION,
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "assets mount failed: %s", esp_err_to_name(err));
        return err;
    }

    s_assets_mounted = true;
    size_t total = 0;
    size_t used = 0;
    esp_spiffs_info(BSP_ASSETS_PARTITION, &total, &used);
    ESP_LOGI(TAG, "assets mounted at %s (%u/%u bytes)",
             BSP_ASSETS_MOUNT_POINT, (unsigned)used, (unsigned)total);
    return ESP_OK;
}

esp_err_t bsp_storage_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_storage_nvs_init(), TAG, "NVS init");

    esp_err_t err = bsp_storage_assets_mount();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "continuing without assets");
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        err = bsp_storage_sd_mount();
        if (err == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "continuing without SD card");
    }

    return ESP_OK;
}

const char *bsp_storage_sd_path(void)
{
    return BSP_SD_MOUNT_POINT;
}

const char *bsp_storage_assets_path(void)
{
    return BSP_ASSETS_MOUNT_POINT;
}
