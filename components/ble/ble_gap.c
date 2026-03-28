#include "ble_gap.h"
#include "ble_profile.h"

#include "host/ble_gap.h"
#include "esp_log.h"

uint16_t ble_conn_handle = 0xFFFF;

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            ble_conn_handle = event->connect.conn_handle;
            ESP_LOGI("BLE", "Connected");
        }
        else
        {
            ESP_LOGI("BLE", "Connect fail");
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("BLE", "Disconnected");
        ble_conn_handle = 0xFFFF;
        ble_app_advertise();
        break;

    default:
        break;
    }

    return 0;
}

void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(0, NULL, BLE_HS_FOREVER,
                      &adv_params, gap_event, NULL);
}

void ble_app_gap_init(void)
{
    ble_app_advertise();
}