#include "wifi_storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

#define NVS_NAMESPACE "wifi"
static const char *TAG = "wifi_storage";

bool wifi_storage_save(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for write");
        return false;
    }

    esp_err_t ret = nvs_set_str(nvs, "SSID", ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID");
    }
    
    ret = nvs_set_str(nvs, "PASS", pass);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save PASS");
    }
    
    nvs_commit(nvs);
    nvs_close(nvs);
    
    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Saved credentials - SSID: %s", ssid);
    } else {
        ESP_LOGI(TAG, "Cleared credentials");
    }
    
    return true;
}

bool wifi_storage_load(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t len;
    
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    
    len = 32;
    if (nvs_get_str(nvs, "SSID", ssid, &len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }
    
    len = 64;
    if (nvs_get_str(nvs, "PASS", pass, &len) != ESP_OK) {
        nvs_close(nvs);
        return false;
    }
    
    nvs_close(nvs);
    
    if (strlen(ssid) > 0) {
        ESP_LOGI(TAG, "Loaded credentials - SSID: %s", ssid);
    }
    
    return true;
}