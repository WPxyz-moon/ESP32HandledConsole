idf_component_register(
    SRCS "imu.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c esp_timer
    PRIV_REQUIRES bsp_board
)
