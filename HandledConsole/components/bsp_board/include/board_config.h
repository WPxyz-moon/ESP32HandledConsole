#pragma once

/*
 * Board: ESP32-S3-WROOM-1U-N16R8 + ST7789 240x280 + 8 keys
 *
 * Pin map source (authoritative):
 *   Gerber_PCB1_2026-10-27/FlyingProbeTesting.json
 *   docs/05-PCB核对与改板建议.md
 *
 * Keep every pin number in this header. Application and driver code must use
 * these macros instead of raw GPIO numbers.
 */

#define BSP_BOARD_NAME "ESP32-S3 Handheld Rev.A"

/* LCD - SPI2 */
#define BSP_LCD_SCK        21
#define BSP_LCD_MOSI       14
#define BSP_LCD_CS         42
#define BSP_LCD_DC         43
#define BSP_LCD_RST        13
#define BSP_LCD_BL         44
#define BSP_LCD_WIDTH      240
#define BSP_LCD_HEIGHT     280
#define BSP_LCD_Y_OFFSET   20

/* microSD - SPI3, no card-detect signal */
#define BSP_SD_CS          38
#define BSP_SD_CLK         40
#define BSP_SD_MOSI        39
#define BSP_SD_MISO        41

/* I2C: LSM6DS3TR-C only, no interrupt pin */
#define BSP_I2C_SDA        1
#define BSP_I2C_SCL        2
#define BSP_IMU_ADDR       0x6A

/* Keys: active low, external/internal pull-up.
 * GPIO macros use the BSP_KEY_GPIO_ prefix so they don't collide with the
 * bsp_key_id_t enum names (BSP_KEY_UP, BSP_KEY_A, ...). */
#define BSP_KEY_GPIO_UP    16
#define BSP_KEY_GPIO_DOWN  15
#define BSP_KEY_GPIO_LEFT  17
#define BSP_KEY_GPIO_RIGHT 7
#define BSP_KEY_GPIO_A     4
#define BSP_KEY_GPIO_B     5
#define BSP_KEY_GPIO_START 6
#define BSP_KEY_GPIO_CTRL  8

/* Audio */
#define BSP_AMP_DIN        3
#define BSP_AMP_BCLK       46
#define BSP_AMP_LRC        9
#define BSP_MIC_DATA       10
#define BSP_MIC_BCLK       11
#define BSP_MIC_WS         12

/* Other */
#define BSP_LED            18
#define BSP_LED_ACTIVE_LOW 1
#define BSP_BOOT           0

/* LCD tuning. Change these only after checking the actual panel. */
#define BSP_LCD_PIXEL_CLOCK_HZ   (40 * 1000 * 1000)
#define BSP_LCD_BACKLIGHT_PWM_HZ 5000
#define BSP_LCD_INVERT_COLOR     1
#define BSP_LCD_MIRROR_X         0
#define BSP_LCD_MIRROR_Y         0
#define BSP_LCD_SWAP_XY          0
#define BSP_LCD_RGB_ORDER_BGR    0
/* Input buffers use little-endian RGB565; the driver swaps bytes for ST7789. */
#define BSP_DISPLAY_SWAP_BYTES   1

/* Storage mount points */
#define BSP_SD_MOUNT_POINT       "/sdcard"
#define BSP_ASSETS_MOUNT_POINT   "/assets"
#define BSP_ASSETS_PARTITION     "assets"

/* Bus speeds */
#define BSP_I2C_SPEED_HZ         400000
#define BSP_SD_SPI_FREQ_KHZ      20000

/* Audio */
#define BSP_MIC_SAMPLE_RATE      16000
#define BSP_SPK_SAMPLE_RATE      16000
#define BSP_AUDIO_RING_BYTES     4096

/* Input */
#define BSP_KEY_DEBOUNCE_MS      20
#define BSP_KEY_LONG_PRESS_MS    1000
