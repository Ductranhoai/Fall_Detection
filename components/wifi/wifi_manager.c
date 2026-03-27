/**
 * Wifi : core wifi
 */
#include "wifi_manager.h"
#include "wifi_storage.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_mgr";

static bool s_connected = false;
static char s_ssid[32] = {0};

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "WiFi started");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            ESP_LOGW(TAG, "Disconnected");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        ESP_LOGI(TAG, "Got IP");
    }
}

void wifi_manager_init(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // load saved wifi
    char ssid[32], pass[64];
    if (wifi_storage_load(ssid, pass)) {
        wifi_manager_connect(ssid, pass);
    }
}

void wifi_manager_connect(const char *ssid, const char *pass)
{
    wifi_config_t cfg = {0};

    strcpy((char*)cfg.sta.ssid, ssid);
    strcpy((char*)cfg.sta.password, pass);

    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    strncpy(s_ssid, ssid, sizeof(s_ssid));

    wifi_storage_save(ssid, pass);

    ESP_LOGI(TAG, "Connecting to %s", ssid);
}

void wifi_manager_disconnect(void)
{
    esp_wifi_disconnect();
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

const char* wifi_manager_get_ssid(void)
{
    return s_ssid;
}