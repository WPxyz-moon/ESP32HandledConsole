idf_component_register(
    SRCS "ota_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES svc_fw_role app_update esp_partition
)
