#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c.h"

// MPU6050 I2C address
#define MPU6050_ADDR         0x68

// MPU6050 registers
#define MPU6050_WHO_AM_I     0x75
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H  0x43
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C

// Configuration options
typedef enum {
    MPU_ACCEL_RANGE_2G  = 0x00,
    MPU_ACCEL_RANGE_4G  = 0x08,
    MPU_ACCEL_RANGE_8G  = 0x10,
    MPU_ACCEL_RANGE_16G = 0x18
} mpu_accel_range_t;

typedef enum {
    MPU_GYRO_RANGE_250  = 0x00,
    MPU_GYRO_RANGE_500  = 0x08,
    MPU_GYRO_RANGE_1000 = 0x10,
    MPU_GYRO_RANGE_2000 = 0x18
} mpu_gyro_range_t;

typedef enum {
    MPU_DLPF_BW_260 = 0x00,  // 260Hz, delay 0ms
    MPU_DLPF_BW_184 = 0x01,  // 184Hz, delay 2.0ms
    MPU_DLPF_BW_94  = 0x02,  // 94Hz,  delay 3.0ms
    MPU_DLPF_BW_44  = 0x03,  // 44Hz,  delay 4.9ms
    MPU_DLPF_BW_21  = 0x04,  // 21Hz,  delay 8.5ms
    MPU_DLPF_BW_10  = 0x05,  // 10Hz,  delay 13.8ms
    MPU_DLPF_BW_5   = 0x06   // 5Hz,   delay 19.0ms
} mpu_dlpf_bandwidth_t;

// Sensor data structure
typedef struct {
    // Raw data
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    
    // Converted data
    float accel_x, accel_y, accel_z;  // g
    float gyro_x, gyro_y, gyro_z;     // deg/s
    float pitch, roll;                 // degrees
    float temperature;                 // degrees Celsius
    
    // Timestamp
    uint32_t timestamp_ms;
} mpu6050_data_t;

// Configuration structure
typedef struct {
    i2c_port_t i2c_port;
    uint32_t i2c_freq_hz;
    uint8_t sda_pin;
    uint8_t scl_pin;
    mpu_accel_range_t accel_range;
    mpu_gyro_range_t gyro_range;
    mpu_dlpf_bandwidth_t dlpf_bandwidth;
    uint32_t sample_rate_hz;  // 4Hz - 1000Hz
    bool enable_fifo;
    uint16_t fifo_buffer_size;
} mpu6050_config_t;

// Callback function type for data ready event
typedef void (*mpu_data_callback_t)(mpu6050_data_t *data);

// Function prototypes
esp_err_t mpu6050_init(const mpu6050_config_t *config);
esp_err_t mpu6050_deinit(void);
esp_err_t mpu6050_read(mpu6050_data_t *data);
esp_err_t mpu6050_calc_angles(mpu6050_data_t *data);
esp_err_t mpu6050_calibrate(void);
void mpu6050_set_data_callback(mpu_data_callback_t callback);
void mpu6050_start_continuous_read(uint32_t interval_ms);
void mpu6050_stop_continuous_read(void);
bool mpu6050_is_initialized(void);

// Configuration helper
mpu6050_config_t mpu6050_get_default_config(void);