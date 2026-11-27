#include "bsp_imu.h"
#include "board_config.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define IMU_REG_WHO_AM_I 0x0F
#define IMU_WHO_AM_I_VAL 0x6A
#define IMU_REG_CTRL1_XL 0x10
#define IMU_REG_CTRL2_G  0x11
#define IMU_REG_CTRL3_C  0x12
#define IMU_REG_OUT_G    0x22
#define IMU_OUT_BYTES    12

#define IMU_TASK_STACK 3072
#define IMU_TASK_PRIO  3
#define IMU_I2C_TIMEOUT_MS 100

/* ±2 g -> 0.061 mg/LSB, 245 dps -> 8.75 mdps/LSB */
#define IMU_ACCEL_G_PER_LSB  0.000061f
#define IMU_GYRO_DPS_PER_LSB 0.00875f

static const char *TAG = "bsp_imu";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_exit_sem;
static bsp_imu_sample_t s_latest;
static bsp_imu_raw_t s_latest_raw;
static bool s_has_sample;
static volatile bool s_run;
static uint32_t s_period_ms = 10;
static bsp_imu_cb_t s_cb;
static void *s_cb_ctx;
static bool s_inited;
static bsp_imu_stats_t s_stats;

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), IMU_I2C_TIMEOUT_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len,
                                       IMU_I2C_TIMEOUT_MS);
}

static void convert(const bsp_imu_raw_t *raw, bsp_imu_sample_t *out)
{
    out->ax_g = raw->ax * IMU_ACCEL_G_PER_LSB;
    out->ay_g = raw->ay * IMU_ACCEL_G_PER_LSB;
    out->az_g = raw->az * IMU_ACCEL_G_PER_LSB;
    out->gx_dps = raw->gx * IMU_GYRO_DPS_PER_LSB;
    out->gy_dps = raw->gy * IMU_GYRO_DPS_PER_LSB;
    out->gz_dps = raw->gz * IMU_GYRO_DPS_PER_LSB;
    out->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

static void imu_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (s_run) {
        uint8_t buf[IMU_OUT_BYTES];
        esp_err_t err = reg_read(IMU_REG_OUT_G, buf, sizeof(buf));
        if (err == ESP_OK) {
            bsp_imu_raw_t raw = {
                .gx = (int16_t)((buf[1] << 8) | buf[0]),
                .gy = (int16_t)((buf[3] << 8) | buf[2]),
                .gz = (int16_t)((buf[5] << 8) | buf[4]),
                .ax = (int16_t)((buf[7] << 8) | buf[6]),
                .ay = (int16_t)((buf[9] << 8) | buf[8]),
                .az = (int16_t)((buf[11] << 8) | buf[10]),
            };
            bsp_imu_sample_t sample;
            convert(&raw, &sample);

            if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) == pdTRUE) {
                s_latest = sample;
                s_latest_raw = raw;
                s_has_sample = true;
                xSemaphoreGive(s_lock);
            }

            s_stats.samples++;
            bsp_imu_cb_t cb = s_cb;
            void *ctx = s_cb_ctx;
            if (cb != NULL) {
                cb(&sample, ctx);
            }
        } else {
            s_stats.read_errors++;
            if ((s_stats.read_errors % 100U) == 1U) {
                ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(s_period_ms));
    }

    if (s_exit_sem != NULL) {
        xSemaphoreGive(s_exit_sem);
    }
    vTaskDelete(NULL);
}

static void imu_cleanup(void)
{
    if (s_dev != NULL) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    if (s_bus != NULL) {
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
    }
    if (s_lock != NULL) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
    }
    if (s_exit_sem != NULL) {
        vSemaphoreDelete(s_exit_sem);
        s_exit_sem = NULL;
    }
}

esp_err_t bsp_imu_init(uint32_t sample_hz)
{
    if (s_inited) {
        return ESP_OK;
    }

    if (sample_hz == 0) {
        sample_hz = 100;
    }
    if (sample_hz < 10) {
        sample_hz = 10;
    } else if (sample_hz > 200) {
        sample_hz = 200;
    }
    s_period_ms = 1000U / sample_hz;
    if (s_period_ms == 0) {
        s_period_ms = 1;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c bus failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_IMU_ADDR,
        .scl_speed_hz = BSP_I2C_SPEED_HZ,
        .scl_wait_us = 0,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "imu device failed: %s", esp_err_to_name(err));
        imu_cleanup();
        return err;
    }

    uint8_t who = 0;
    err = reg_read(IMU_REG_WHO_AM_I, &who, 1);
    if (err != ESP_OK || who != IMU_WHO_AM_I_VAL) {
        s_stats.init_errors++;
        ESP_LOGE(TAG, "WHO_AM_I mismatch: err=%s value=0x%02x",
                 esp_err_to_name(err), who);
        imu_cleanup();
        return ESP_ERR_NOT_FOUND;
    }

    /* BDU + IF_INC */
    err = reg_write(IMU_REG_CTRL3_C, 0x44);
    if (err == ESP_OK) {
        /* 104 Hz, ±2 g */
        err = reg_write(IMU_REG_CTRL1_XL, 0x40);
    }
    if (err == ESP_OK) {
        /* 104 Hz, 245 dps */
        err = reg_write(IMU_REG_CTRL2_G, 0x40);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register init failed: %s", esp_err_to_name(err));
        imu_cleanup();
        return err;
    }

    s_lock = xSemaphoreCreateMutex();
    s_exit_sem = xSemaphoreCreateBinary();
    if (s_lock == NULL || s_exit_sem == NULL) {
        imu_cleanup();
        return ESP_ERR_NO_MEM;
    }

    s_run = true;
    if (xTaskCreate(imu_task, "bsp_imu", IMU_TASK_STACK, NULL, IMU_TASK_PRIO,
                    &s_task) != pdPASS) {
        s_run = false;
        imu_cleanup();
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG, "LSM6DS3TR-C ready at %u Hz", sample_hz);
    return ESP_OK;
}

esp_err_t bsp_imu_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    s_run = false;
    if (s_task != NULL && s_exit_sem != NULL) {
        /* Wait for the task to leave the I2C busy window and delete itself. */
        xSemaphoreTake(s_exit_sem, pdMS_TO_TICKS(500));
        s_task = NULL;
    }
    imu_cleanup();

    s_has_sample = false;
    s_inited = false;
    return ESP_OK;
}

esp_err_t bsp_imu_read_latest(bsp_imu_sample_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited || s_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_has_sample) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    *out = s_latest;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t bsp_imu_read_raw(bsp_imu_raw_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_inited || s_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    if (!s_has_sample) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    *out = s_latest_raw;
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t bsp_imu_set_callback(bsp_imu_cb_t cb, void *ctx)
{
    s_cb = cb;
    s_cb_ctx = ctx;
    return ESP_OK;
}

esp_err_t bsp_imu_get_stats(bsp_imu_stats_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = s_stats;
    return ESP_OK;
}
