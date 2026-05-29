#pragma once
#include "mpu6050.h"

// MPU6050 Configuration structure
typedef struct {
    // I2C pins and configuration
    uint8_t sda_pin;           // I2C SDA pin (default: 21)
    uint8_t scl_pin;           // I2C SCL pin (default: 22)
    i2c_port_t i2c_port;       // I2C port number (default: I2C_NUM_0)
    uint32_t i2c_freq;         // I2C frequency in Hz (default: 400000)
    
    // Sensor ranges
    mpu_accel_range_t accel_range;  // Accelerometer full scale range
    mpu_gyro_range_t gyro_range;    // Gyroscope full scale range
    mpu_dlpf_bandwidth_t dlpf_bandwidth;  // Digital Low Pass Filter bandwidth
    
    // Sampling configuration
    uint32_t sample_rate_hz;   // Internal sample rate (4-1000 Hz)
    uint32_t read_interval_ms; // How often to read data in continuous mode
    
    // Optional features
    bool enable_fifo;          // Enable FIFO buffer
    bool enable_calibration;   // Auto-calibrate on startup
} mpu_config_t;

// Default configuration for fall detection application
static inline mpu_config_t mpu_get_default_config(void)
{
    mpu_config_t config = {
        .sda_pin = 5,
        .scl_pin = 4,
        .i2c_port = I2C_NUM_0,
        .i2c_freq = 400000,
        .accel_range = MPU_ACCEL_RANGE_2G,      // ±2g for better sensitivity
        .gyro_range = MPU_GYRO_RANGE_250,       // ±250°/s for fall detection
        .dlpf_bandwidth = MPU_DLPF_BW_94,       // 21Hz filter for noise reduction
        .sample_rate_hz = 200,                  // 100 Hz sampling
        .read_interval_ms = 10,                 // Read every 50ms (20 Hz)
        .enable_fifo = false,                   // FIFO not needed for basic use
        .enable_calibration = true              // Auto-calibrate on startup
    };
    return config;
}

// Alternative configuration for high-speed applications
static inline mpu_config_t mpu_get_fast_config(void)
{
    mpu_config_t config = {
        .sda_pin = 5,
        .scl_pin = 4,
        .i2c_port = I2C_NUM_0,
        .i2c_freq = 400000,
        .accel_range = MPU_ACCEL_RANGE_4G,      // ±4g for higher range
        .gyro_range = MPU_GYRO_RANGE_500,       // ±500°/s for faster movements
        .dlpf_bandwidth = MPU_DLPF_BW_260,      // 260Hz for high speed
        .sample_rate_hz = 1000,                 // 1 kHz sampling
        .read_interval_ms = 10,                 // Read every 10ms (100 Hz)
        .enable_fifo = true,                    // Enable FIFO for high speed
        .enable_calibration = true
    };
    return config;
}

// Alternative configuration for low-power applications
static inline mpu_config_t mpu_get_lowpower_config(void)
{
    mpu_config_t config = {
        .sda_pin = 21,
        .scl_pin = 22,
        .i2c_port = I2C_NUM_0,
        .i2c_freq = 100000,                     // Lower I2C speed
        .accel_range = MPU_ACCEL_RANGE_2G,
        .gyro_range = MPU_GYRO_RANGE_250,
        .dlpf_bandwidth = MPU_DLPF_BW_10,       // 10Hz for lower power
        .sample_rate_hz = 20,                   // 20 Hz sampling
        .read_interval_ms = 100,                // Read every 100ms (10 Hz)
        .enable_fifo = false,
        .enable_calibration = false              // Skip calibration to save time
    };
    return config;
}