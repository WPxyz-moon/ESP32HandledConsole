idf_component_register(
    SRCS "power.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_hw_support
    PRIV_REQUIRES bsp_board bsp_input
)
