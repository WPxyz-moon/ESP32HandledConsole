idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs esp_driver_sdspi esp_driver_spi spiffs nvs_flash sdmmc
    PRIV_REQUIRES bsp_board
)
