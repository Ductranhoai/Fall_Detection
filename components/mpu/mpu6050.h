#pragma once
#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define MPU6050_ADDR 0x68
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H 0x43

// Định nghĩa struct với tên đầy đủ
typedef struct mpu6050_data_s
{
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
} mpu6050_data_t;

esp_err_t mpu6050_init(i2c_port_t i2c_num);
esp_err_t mpu6050_read_accel(mpu6050_data_t *data);
esp_err_t mpu6050_read_gyro(mpu6050_data_t *data);
esp_err_t mpu6050_read_all(mpu6050_data_t *data);