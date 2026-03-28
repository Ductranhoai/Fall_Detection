/**
 * wifi_storage.* → lưu SSID/pass vào NVS
 */

#include "wifi_storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

#define NVS_NAMESPACE "wifi"

bool wifi_storage_save(const char *ssid, const char *pass)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK)
        return false;

    nvs_set_str(nvs, "SSID", ssid);
    nvs_set_str(nvs, "PASS", pass);
    nvs_commit(nvs);
    nvs_close(nvs);

    return true;
}

bool wifi_storage_load(char *ssid, char *pass)
{
    nvs_handle_t nvs;
    size_t len;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;

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
    return true;
}