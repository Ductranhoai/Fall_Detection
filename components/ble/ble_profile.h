#pragma once
#include <stdint.h>

// ===== CONFIG =====
#define BLE_DEVICE_NAME "ESP32-BLE"

// UUID đơn giản (16-bit cho nhẹ)
#define BLE_SERVICE_UUID 0xABF0
#define BLE_CHAR_RX_UUID 0xABF1
#define BLE_CHAR_TX_UUID 0xABF2

// ===== GLOBAL =====
extern uint16_t ble_conn_handle;
extern uint16_t tx_handle;

// ===== CALLBACK APP =====
void ble_on_receive(uint8_t *data, uint16_t len);