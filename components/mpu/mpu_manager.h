#pragma once
#include "mpu6050.h"
#include <stdbool.h>

// Configuration structure
typedef struct
{
    uint8_t sda_pin;
    uint8_t scl_pin;
    i2c_port_t i2c_port;
    uint32_t i2c_freq;
    mpu_accel_range_t accel_range;
    mpu_gyro_range_t gyro_range;
    mpu_dlpf_bandwidth_t dlpf_bandwidth;
    uint32_t sample_rate_hz;
    uint32_t read_interval_ms;
    bool enable_fifo;
    bool enable_calibration;
} mpu_config_t;

// Function prototypes
esp_err_t mpu_manager_init(const mpu_config_t *config);
esp_err_t mpu_manager_init_default(void);
esp_err_t mpu_manager_deinit(void);
esp_err_t mpu_manager_get_data(mpu6050_data_t *data);
void mpu_manager_start_monitoring(mpu_data_callback_t callback);
void mpu_manager_stop_monitoring(void);
const mpu_config_t *mpu_manager_get_config(void);
bool mpu_manager_is_initialized(void);
bool mpu_manager_is_monitoring(void);

// Default configuration
static inline mpu_config_t mpu_get_default_config(void)
{
    mpu_config_t config = {
        .sda_pin = 21,
        .scl_pin = 22,
        .i2c_port = I2C_NUM_0,
        .i2c_freq = 400000,
        .accel_range = MPU_ACCEL_RANGE_2G,
        .gyro_range = MPU_GYRO_RANGE_250,
        .dlpf_bandwidth = MPU_DLPF_BW_21,
        .sample_rate_hz = 100,
        .read_interval_ms = 100,
        .enable_fifo = false,
        .enable_calibration = true};
    return config;
}