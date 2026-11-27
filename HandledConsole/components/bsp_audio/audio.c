#include "bsp_audio.h"
#include "board_config.h"

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

#include <string.h>

#define AUDIO_TASK_STACK 4096
#define AUDIO_TASK_PRIO  7
#define AUDIO_IO_TIMEOUT_MS 1000
#define AUDIO_TMP_BYTES 512

static const char *TAG = "bsp_audio";

static i2s_chan_handle_t s_rx;
static i2s_chan_handle_t s_tx;
static StreamBufferHandle_t s_rx_ring;
static StreamBufferHandle_t s_tx_ring;
static TaskHandle_t s_rx_task;
static TaskHandle_t s_tx_task;
static volatile bool s_run;
static volatile bool s_tx_active;
static volatile uint8_t s_volume = 100;
static uint32_t s_spk_rate = BSP_SPK_SAMPLE_RATE;
static bool s_inited;
static bsp_audio_stats_t s_stats;

static esp_err_t mic_channel_init(void)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, NULL, &s_rx),
                        TAG, "mic channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_MIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)BSP_MIC_BCLK,
            .ws = (gpio_num_t)BSP_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)BSP_MIC_DATA,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_rx, &std_cfg),
                        TAG, "mic std mode");
    return i2s_channel_enable(s_rx);
}

static esp_err_t spk_channel_init(void)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, NULL),
                        TAG, "spk channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(s_spk_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)BSP_AMP_BCLK,
            .ws = (gpio_num_t)BSP_AMP_LRC,
            .dout = (gpio_num_t)BSP_AMP_DIN,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg),
                        TAG, "spk std mode");
    return i2s_channel_enable(s_tx);
}

static void audio_channels_cleanup(void)
{
    if (s_rx != NULL) {
        i2s_channel_disable(s_rx);
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
    if (s_tx != NULL) {
        i2s_channel_disable(s_tx);
        i2s_del_channel(s_tx);
        s_tx = NULL;
    }
    if (s_rx_ring != NULL) {
        vStreamBufferDelete(s_rx_ring);
        s_rx_ring = NULL;
    }
    if (s_tx_ring != NULL) {
        vStreamBufferDelete(s_tx_ring);
        s_tx_ring = NULL;
    }
}

static void mic_task(void *arg)
{
    (void)arg;
    uint8_t tmp[AUDIO_TMP_BYTES];

    while (s_run) {
        size_t bytes = 0;
        esp_err_t err = i2s_channel_read(s_rx, tmp, sizeof(tmp), &bytes,
                                         AUDIO_IO_TIMEOUT_MS);
        if (err != ESP_OK || bytes == 0) {
            continue;
        }

        s_stats.bytes_rx += bytes;
        size_t sent = xStreamBufferSend(s_rx_ring, tmp, bytes, 0);
        if (sent < bytes) {
            s_stats.mic_overrun += (uint32_t)(bytes - sent);
        }
    }

    vTaskDelete(NULL);
}

static void apply_volume(int16_t *samples, size_t count)
{
    uint32_t volume = s_volume;
    if (volume == 100) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        int32_t v = ((int32_t)samples[i] * (int32_t)volume) / 100;
        if (v > 32767) {
            v = 32767;
        } else if (v < -32768) {
            v = -32768;
        }
        samples[i] = (int16_t)v;
    }
}

static void spk_task(void *arg)
{
    (void)arg;
    int16_t tmp[AUDIO_TMP_BYTES / sizeof(int16_t)];

    while (s_run) {
        size_t bytes = xStreamBufferReceive(s_tx_ring, tmp, sizeof(tmp),
                                            pdMS_TO_TICKS(20));
        if (bytes == 0) {
            if (s_tx_active) {
                s_tx_active = false;
                s_stats.spk_underrun++;
            }
            /* Keep the I2S clock fed with silence while idle. */
            memset(tmp, 0, sizeof(tmp));
            bytes = sizeof(tmp);
        }

        apply_volume(tmp, bytes / sizeof(int16_t));

        size_t written = 0;
        if (i2s_channel_write(s_tx, tmp, bytes, &written,
                              AUDIO_IO_TIMEOUT_MS) == ESP_OK) {
            s_stats.bytes_tx += written;
        }
    }

    vTaskDelete(NULL);
}

esp_err_t bsp_audio_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    esp_err_t err = mic_channel_init();
    if (err != ESP_OK) {
        return err;
    }
    err = spk_channel_init();
    if (err != ESP_OK) {
        audio_channels_cleanup();
        return err;
    }

    s_rx_ring = xStreamBufferCreate(BSP_AUDIO_RING_BYTES, 1);
    s_tx_ring = xStreamBufferCreate(BSP_AUDIO_RING_BYTES, 1);
    if (s_rx_ring == NULL || s_tx_ring == NULL) {
        audio_channels_cleanup();
        return ESP_ERR_NO_MEM;
    }

    s_run = true;
    if (xTaskCreate(mic_task, "bsp_audio_rx", AUDIO_TASK_STACK, NULL,
                    AUDIO_TASK_PRIO, &s_rx_task) != pdPASS) {
        s_run = false;
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(spk_task, "bsp_audio_tx", AUDIO_TASK_STACK, NULL,
                    AUDIO_TASK_PRIO, &s_tx_task) != pdPASS) {
        s_run = false;
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG, "I2S ready (mic %d Hz, spk %u Hz)",
             BSP_MIC_SAMPLE_RATE, (unsigned)s_spk_rate);
    return ESP_OK;
}

esp_err_t bsp_audio_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    s_run = false;
    if (s_rx != NULL) {
        i2s_channel_disable(s_rx);
    }
    if (s_tx != NULL) {
        i2s_channel_disable(s_tx);
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    if (s_rx != NULL) {
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
    if (s_tx != NULL) {
        i2s_del_channel(s_tx);
        s_tx = NULL;
    }
    if (s_rx_ring != NULL) {
        vStreamBufferDelete(s_rx_ring);
        s_rx_ring = NULL;
    }
    if (s_tx_ring != NULL) {
        vStreamBufferDelete(s_tx_ring);
        s_tx_ring = NULL;
    }

    s_rx_task = NULL;
    s_tx_task = NULL;
    s_tx_active = false;
    s_inited = false;
    return ESP_OK;
}

esp_err_t bsp_audio_mic_read(void *buf, size_t bytes, size_t *out_bytes,
                             uint32_t timeout_ms)
{
    if (!s_inited || s_rx_ring == NULL || buf == NULL || out_bytes == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t got = xStreamBufferReceive(s_rx_ring, buf, bytes,
                                      pdMS_TO_TICKS(timeout_ms));
    if (got == 0) {
        return ESP_ERR_TIMEOUT;
    }
    *out_bytes = got;
    return ESP_OK;
}

esp_err_t bsp_audio_spk_write(const void *buf, size_t bytes,
                              uint32_t timeout_ms)
{
    if (!s_inited || s_tx_ring == NULL || buf == NULL || bytes == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    s_tx_active = true;
    size_t sent = xStreamBufferSend(s_tx_ring, buf, bytes,
                                    pdMS_TO_TICKS(timeout_ms));
    if (sent == 0) {
        return ESP_ERR_TIMEOUT;
    }
    return sent == bytes ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t bsp_audio_set_volume(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    s_volume = percent;
    return ESP_OK;
}

esp_err_t bsp_audio_set_spk_sample_rate(uint32_t hz)
{
    if (!s_inited || s_tx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (hz < 8000 || hz > 48000) {
        return ESP_ERR_INVALID_ARG;
    }
    if (hz == s_spk_rate) {
        return ESP_OK;
    }

    i2s_channel_disable(s_tx);
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(hz);
    esp_err_t err = i2s_channel_reconfig_std_clock(s_tx, &clk_cfg);
    if (err == ESP_OK) {
        s_spk_rate = hz;
        xStreamBufferReset(s_tx_ring);
        s_tx_active = false;
    }
    i2s_channel_enable(s_tx);
    return err;
}

esp_err_t bsp_audio_get_stats(bsp_audio_stats_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = s_stats;
    return ESP_OK;
}
