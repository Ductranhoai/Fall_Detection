idf_component_register(
    SRCS "wifi_manager.c" "wifi_storage.c" "wifi_cli.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event esp_netif console
)