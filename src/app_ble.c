#include "ble_profile.h"
#include "ble.h"
#include <stdio.h>
#include <string.h>

// ===== APP xử lý data từ điện thoại =====
__attribute__((noinline)) void ble_on_receive(uint8_t *data, uint16_t len)
{
    printf("RX: %.*s\n", len, data);

    // ví dụ: echo lại
    ble_send(data, len);

    // ví dụ nâng cấp:
    if (strncmp((char *)data, "ping", len) == 0)
    {
        char *resp = "pong";
        ble_send((uint8_t *)resp, strlen(resp));
    }
}