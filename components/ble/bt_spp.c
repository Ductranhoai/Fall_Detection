#if CONFIG_IDF_TARGET_ESP32
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#include "bt_spp.h"

static const char *TAG = "BT_SPP";

static uint32_t spp_handle = 0;
static bool is_connected = false;

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(TAG, "SPP INIT");

        esp_bt_gap_set_device_name("ESP32_SPP");

        esp_bt_gap_set_scan_mode(
            ESP_BT_CONNECTABLE,
            ESP_BT_GENERAL_DISCOVERABLE
        );

        esp_spp_start_srv(
            ESP_SPP_SEC_AUTHENTICATE,
            ESP_SPP_ROLE_SLAVE,
            0,
            "SPP_SERVER"
        );
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        spp_handle = param->srv_open.handle;
        is_connected = true;

        ESP_LOGI(TAG, "Client connected");

        const char *msg = "BT connected\r\n";
        esp_spp_write(spp_handle, strlen(msg), (uint8_t *)msg);
        break;

    case ESP_SPP_CLOSE_EVT:
        ESP_LOGW(TAG, "Client disconnected");

        spp_handle = 0;
        is_connected = false;
        break;

    case ESP_SPP_DATA_IND_EVT:
        ESP_LOGI(TAG, "RX: %.*s",
                 param->data_ind.len,
                 (char *)param->data_ind.data);

        // echo
        esp_spp_write(
            param->data_ind.handle,
            param->data_ind.len,
            param->data_ind.data
        );
        break;

    default:
        break;
    }
}

void bt_spp_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // security fix
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));
    ESP_ERROR_CHECK(esp_spp_init(ESP_SPP_MODE_CB));

    ESP_LOGI(TAG, "Bluetooth SPP started");
}

bool bt_spp_is_connected(void)
{
    return is_connected;
}

esp_err_t bt_spp_send(const uint8_t *data, size_t len)
{
    if (!is_connected || spp_handle == 0)
        return ESP_FAIL;

    return esp_spp_write(spp_handle, len, (uint8_t *)data);
}
#endif