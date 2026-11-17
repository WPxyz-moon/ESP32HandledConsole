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

/* Keys: active low, external/internal pull-up */
#define BSP_KEY_UP         16
#define BSP_KEY_DOWN       15
#define BSP_KEY_LEFT       17
#define BSP_KEY_RIGHT      7
#define BSP_KEY_A          4
#define BSP_KEY_B          5
#define BSP_KEY_START      6
#define BSP_KEY_CTRL       8
#define BSP_KEY_COUNT      8

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
