idf_component_register(
    SRCS "wifi_manager.c" "wifi_storage.c" "wifi_cli.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash esp_event esp_netif
)