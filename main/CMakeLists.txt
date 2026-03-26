idf_component_register(
    SRCS "main.c" "servo.c" "ble_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash bt driver
)