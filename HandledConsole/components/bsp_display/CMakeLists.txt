idf_component_register(
    SRCS "display.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_spi esp_driver_ledc esp_driver_gpio esp_timer heap
    PRIV_REQUIRES bsp_board
)
