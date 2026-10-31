#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

#define MAX30102_ADDR 0x57

// REGISTERS
#define MAX30102_REG_INTR_STATUS_1   0x00
#define MAX30102_REG_INTR_STATUS_2   0x01
#define MAX30102_REG_INTR_ENABLE_1   0x02
#define MAX30102_REG_INTR_ENABLE_2   0x03
#define MAX30102_REG_FIFO_WR_PTR     0x04
#define MAX30102_REG_FIFO_OVF_CTR    0x05
#define MAX30102_REG_FIFO_RD_PTR     0x06
#define MAX30102_REG_FIFO_DATA       0x07
#define MAX30102_REG_FIFO_CONFIG     0x08
#define MAX30102_REG_MODE_CONFIG     0x09
#define MAX30102_REG_SPO2_CONFIG     0x0A
#define MAX30102_REG_LED1_PA         0x0C
#define MAX30102_REG_LED2_PA         0x0D
#define MAX30102_REG_MULTI_LED_CTRL1 0x11
#define MAX30102_REG_MULTI_LED_CTRL2 0x12
#define MAX30102_REG_PART_ID         0xFF

typedef struct {
    uint32_t red;
    uint32_t ir;
    float heart_rate;
    float spo2;
    bool finger_detected;
} max30102_data_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t max30102_init(i2c_port_t port, uint8_t sda, uint8_t scl);
esp_err_t max30102_read(max30102_data_t *data);
esp_err_t max30102_get_heart_rate(float *hr);
esp_err_t max30102_get_spo2(float *spo2);
bool max30102_is_finger_present(void);
void max30102_deinit(void);

#ifdef __cplusplus
}
#endif

#endif