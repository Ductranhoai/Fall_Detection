#include "ble.h"
#include "ble_profile.h"
#include "ble_gap.h"
#include "ble_gatt.h"

#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gatt.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_hs.h"

void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ===== API SEND =====
__attribute__((noinline)) void ble_send(uint8_t *data, uint16_t len)
{
    if (ble_conn_handle == 0xFFFF)
        return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);

    ble_gatts_notify_custom(ble_conn_handle, tx_handle, om);
}

// ===== INIT =====
void ble_init(void)
{
    nvs_flash_init();

    nimble_port_init();

    ble_app_gap_init();
    ble_gatt_init();

    nimble_port_freertos_init(ble_host_task);
}