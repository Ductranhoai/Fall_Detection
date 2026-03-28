idf_component_register(
    SRCS
        "ble.c"
        "ble_gap.c"
        "ble_gatt.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt
)