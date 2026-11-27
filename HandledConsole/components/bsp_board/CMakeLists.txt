idf_component_register(
    SRCS "board.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
             bsp_display bsp_input bsp_imu bsp_audio bsp_storage bsp_power
)
