#include "bt_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "wifi_manager.h"

static const char *TAG = "BT";
#define DEVICE_NAME "FallDetector"

/* Nordic UART Service UUIDs (tuong thich moi app BLE) */
static const ble_uuid128_t s_svc_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
static const ble_uuid128_t s_rx_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
static const ble_uuid128_t s_tx_uuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static uint16_t s_tx_val_handle;
static uint16_t s_rx_val_handle;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_notify_enabled = false;
static uint8_t s_own_addr_type;
static char s_status[32] = "READY";
static portMUX_TYPE s_status_mux = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    char ssid[33];
    char pass[65];
} wifi_cred_t;

static QueueHandle_t s_cred_queue = NULL;

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gap_event_cb(struct ble_gap_event *event, void *arg);

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &s_tx_uuid.u,
                .access_cb = gatt_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                .val_handle = &s_tx_val_handle,
            },
            {
                .uuid = &s_rx_uuid.u,
                .access_cb = gatt_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &s_rx_val_handle,
            },
            {0},
        },
    },
    {0},
};

static void set_status(const char *text)
{
    taskENTER_CRITICAL(&s_status_mux);
    snprintf(s_status, sizeof(s_status), "%s", text);
    taskEXIT_CRITICAL(&s_status_mux);
}

static void wifi_provision_task(void *arg)
{
    wifi_cred_t cred;

    for (;;)
    {
        if (xQueueReceive(s_cred_queue, &cred, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (cred.ssid[0] == '\0')
        {
            ESP_LOGI(TAG, "Clearing WiFi credentials");
            wifi_manager_clear_credentials();
            bt_manager_notify_text("CFG:CLR");
        }
        else
        {
            ESP_LOGI(TAG, "Provisioning WiFi SSID=%s", cred.ssid);
            bt_manager_notify_text("CFG:OK");
            wifi_manager_connect(cred.ssid, cred.pass);
        }
    }
}

static void handle_cfg_write(char *s)
{
    while (*s == ' ' || *s == '\r' || *s == '\n')
    {
        s++;
    }

    if (strncmp(s, "WIFI:", 5) == 0)
    {
        s += 5;
    }

    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' '))
    {
        s[--n] = '\0';
    }

    wifi_cred_t cred = {0};

    if (strcmp(s, "CLR") == 0)
    {
        if (xQueueSend(s_cred_queue, &cred, 0) != pdTRUE)
        {
            bt_manager_notify_text("CFG:BUSY");
        }
        return;
    }

    char *comma = strchr(s, ',');
    if (comma == NULL)
    {
        bt_manager_notify_text("CFG:ERR");
        return;
    }

    *comma = '\0';
    strncpy(cred.ssid, s, sizeof(cred.ssid) - 1);
    strncpy(cred.pass, comma + 1, sizeof(cred.pass) - 1);

    if (cred.ssid[0] == '\0')
    {
        bt_manager_notify_text("CFG:ERR");
        return;
    }

    if (xQueueSend(s_cred_queue, &cred, 0) != pdTRUE)
    {
        bt_manager_notify_text("CFG:BUSY");
    }
}

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
    {
        char status[sizeof(s_status)];
        taskENTER_CRITICAL(&s_status_mux);
        memcpy(status, s_status, sizeof(status));
        taskEXIT_CRITICAL(&s_status_mux);
        return os_mbuf_append(ctxt->om, status, strlen(status));
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        char buf[128];
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len >= sizeof(buf))
        {
            len = sizeof(buf) - 1;
        }
        if (os_mbuf_copydata(ctxt->om, 0, len, buf) != 0)
        {
            return BLE_ATT_ERR_UNLIKELY;
        }
        buf[len] = '\0';
        handle_cfg_write(buf);
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "adv fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &params, gap_event_cb, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY)
    {
        ESP_LOGE(TAG, "adv start failed: %d", rc);
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Client connected");
        }
        else
        {
            ESP_LOGW(TAG, "Connect failed, status=%d", event->connect.status);
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Client disconnected, reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_notify_enabled = false;
        start_advertising();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_tx_val_handle)
        {
            s_notify_enabled = event->subscribe.cur_notify;
            if (s_notify_enabled)
            {
                bt_manager_notify_text("READY");
            }
        }
        break;

    default:
        break;
    }

    return 0;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "ensure_addr failed: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "infer addr failed: %d", rc);
        return;
    }

    ESP_LOGI(TAG, "BLE synced, starting advertising");
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t bt_manager_init(void)
{
    s_cred_queue = xQueueCreate(2, sizeof(wifi_cred_t));
    if (s_cred_queue == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gatt count failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "gatt add failed: %d", rc);
        return ESP_FAIL;
    }

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_att_set_preferred_mtu(247);

    if (xTaskCreate(wifi_provision_task, "wifi_prov", 4096, NULL, 4, NULL) != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }

    nimble_port_freertos_init(host_task);

    ESP_LOGI(TAG, "BLE ready, name=%s", DEVICE_NAME);
    return ESP_OK;
}

bool bt_manager_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

esp_err_t bt_manager_notify_text(const char *text)
{
    if (text == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    set_status(text);

    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_notify_enabled)
    {
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(text, strlen(text));
    if (om == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    int rc = ble_gatts_notify_custom(s_conn_handle, s_tx_val_handle, om);
    return (rc == 0) ? ESP_OK : ESP_FAIL;
}
