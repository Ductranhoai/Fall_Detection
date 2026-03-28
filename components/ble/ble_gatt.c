#include "ble_gatt.h"
#include "ble_profile.h"

#include "host/ble_gatt.h"
#include "host/ble_hs.h"

uint16_t tx_handle;

// ===== RX từ app =====
static int rx_write(uint16_t conn_handle,
                    uint16_t attr_handle,
                    struct ble_gatt_access_ctxt *ctxt,
                    void *arg)
{
    uint8_t *data = ctxt->om->om_data;
    uint16_t len = ctxt->om->om_len;

    ble_on_receive(data, len); // 🔥 gọi sang APP

    return 0;
}

// ===== GATT TABLE =====
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){

            // RX (WRITE)
            {
                .uuid = BLE_UUID16_DECLARE(BLE_CHAR_RX_UUID),
                .access_cb = rx_write,
                .flags = BLE_GATT_CHR_F_WRITE,
            },

            // TX (NOTIFY)
            {
                .uuid = BLE_UUID16_DECLARE(BLE_CHAR_TX_UUID),
                .val_handle = &tx_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },

            {0}},
    },
    {0}};

void ble_gatt_init(void)
{
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
}