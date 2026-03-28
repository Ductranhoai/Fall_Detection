/**
 * BLE hoạt động theo kiểu:
 *       GAP → quảng bá (advertising), connect/disconnect
 *       GATT → dữ liệu (service / characteristic)
 */

#pragma once
#include <stdint.h>

void ble_init(void);
void ble_send(uint8_t *data, uint16_t len);