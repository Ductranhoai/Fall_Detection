idf_component_register(
    SRCS "bt_manager.c" "bt_spp.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash wifi
)
