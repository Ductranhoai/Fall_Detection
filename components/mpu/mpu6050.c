#include "mpu6050.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MPU6050";
static i2c_port_t i2c_port = I2C_NUM_0;

static esp_err_t mpu6050_write_byte(uint8_t reg, uint8_t data)
{
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(i2c_port, MPU6050_ADDR, write_buf, 2, 1000 / portTICK_PERIOD_MS);
}

static esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(i2c_port, MPU6050_ADDR, &reg, 1, data, len, 1000 / portTICK_PERIOD_MS);
}

esp_err_t mpu6050_init(i2c_port_t i2c_num)
{
    i2c_port = i2c_num;

    // Wake up MPU6050
    esp_err_t ret = mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to wake up MPU6050");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Check WHO_AM_I
    uint8_t who_am_i;
    ret = mpu6050_read_bytes(MPU6050_WHO_AM_I, &who_am_i, 1);
    if (ret != ESP_OK || who_am_i != 0x68)
    {
        ESP_LOGE(TAG, "MPU6050 not found. WHO_AM_I = 0x%02X", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    return ESP_OK;
}

esp_err_t mpu6050_read_accel(mpu6050_data_t *data)
{
    uint8_t buffer[6];
    esp_err_t ret = mpu6050_read_bytes(MPU6050_ACCEL_XOUT_H, buffer, 6);
    if (ret != ESP_OK)
        return ret;

    data->ax = (buffer[0] << 8) | buffer[1];
    data->ay = (buffer[2] << 8) | buffer[3];
    data->az = (buffer[4] << 8) | buffer[5];

    // Convert to g (16384 LSB/g)
    data->accel_x = data->ax / 16384.0;
    data->accel_y = data->ay / 16384.0;
    data->accel_z = data->az / 16384.0;

    return ESP_OK;
}

esp_err_t mpu6050_read_gyro(mpu6050_data_t *data)
{
    uint8_t buffer[6];
    esp_err_t ret = mpu6050_read_bytes(MPU6050_GYRO_XOUT_H, buffer, 6);
    if (ret != ESP_OK)
        return ret;

    data->gx = (buffer[0] << 8) | buffer[1];
    data->gy = (buffer[2] << 8) | buffer[3];
    data->gz = (buffer[4] << 8) | buffer[5];

    // Convert to deg/s (131 LSB/deg/s)
    data->gyro_x = data->gx / 131.0;
    data->gyro_y = data->gy / 131.0;
    data->gyro_z = data->gz / 131.0;

    return ESP_OK;
}

esp_err_t mpu6050_read_all(mpu6050_data_t *data)
{
    uint8_t buffer[14];
    esp_err_t ret = mpu6050_read_bytes(MPU6050_ACCEL_XOUT_H, buffer, 14);
    if (ret != ESP_OK)
        return ret;

    data->ax = (buffer[0] << 8) | buffer[1];
    data->ay = (buffer[2] << 8) | buffer[3];
    data->az = (buffer[4] << 8) | buffer[5];
    data->gx = (buffer[8] << 8) | buffer[9];
    data->gy = (buffer[10] << 8) | buffer[11];
    data->gz = (buffer[12] << 8) | buffer[13];

    data->accel_x = data->ax / 16384.0;
    data->accel_y = data->ay / 16384.0;
    data->accel_z = data->az / 16384.0;
    data->gyro_x = data->gx / 131.0;
    data->gyro_y = data->gy / 131.0;
    data->gyro_z = data->gz / 131.0;

    return ESP_OK;
}