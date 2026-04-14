#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

static const char *TAG = "BT_SPP";

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event)
    {
    case ESP_SPP_INIT_EVT:
        ESP_LOGI(TAG, "SPP INIT");

        // set device name
        esp_bt_gap_set_device_name("ESP32_SPP");

        // make discoverable + connectable
        esp_bt_gap_set_scan_mode(
            ESP_BT_CONNECTABLE,
            ESP_BT_GENERAL_DISCOVERABLE
        );

        // start SPP server
        esp_spp_start_srv(
            ESP_SPP_SEC_NONE,
            ESP_SPP_ROLE_SLAVE,
            0,
            "SPP_SERVER"
        );
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(TAG, "Client connected");
        break;

    case ESP_SPP_DATA_IND_EVT:
        ESP_LOGI(TAG, "RX (%d): %.*s",
                 param->data_ind.len,
                 param->data_ind.len,
                 (char *)param->data_ind.data);

        // echo back
        esp_spp_write(
            param->data_ind.handle,
            param->data_ind.len,
            param->data_ind.data
        );
        break;

    case ESP_SPP_CONG_EVT:
        ESP_LOGI(TAG, "Congestion");
        break;

    case ESP_SPP_WRITE_EVT:
        break;

    default:
        break;
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));

    ESP_ERROR_CHECK(esp_spp_init(ESP_SPP_MODE_CB));

    ESP_LOGI(TAG, "Bluetooth SPP started");
}