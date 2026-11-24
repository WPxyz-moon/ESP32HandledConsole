#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t mic_overrun;
    uint32_t spk_underrun;
    uint32_t bytes_rx;
    uint32_t bytes_tx;
} bsp_audio_stats_t;

/*
 * All PCM is signed 16-bit, mono.
 *   MIC: 16 kHz
 *   SPK: 16 kHz by default, configurable to 24 kHz for TTS.
 */
esp_err_t bsp_audio_init(void);
esp_err_t bsp_audio_deinit(void);

esp_err_t bsp_audio_mic_read(void *buf, size_t bytes, size_t *out_bytes,
                             uint32_t timeout_ms);
esp_err_t bsp_audio_spk_write(const void *buf, size_t bytes,
                              uint32_t timeout_ms);

esp_err_t bsp_audio_set_volume(uint8_t percent);
esp_err_t bsp_audio_set_spk_sample_rate(uint32_t hz);

esp_err_t bsp_audio_get_stats(bsp_audio_stats_t *out);

#ifdef __cplusplus
}
#endif
