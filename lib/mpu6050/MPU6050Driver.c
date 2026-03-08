/**
 * @file MPU6050Driver.c
 * @brief Implementation của driver MPU6050
 */

#include "MPU6050Driver.h"
#include "config.h"
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Tag cho logging
static const char *TAG = "MPU6050";

// Biến static để lưu trạng thái
static bool mpu6050_initialized = false;
static float accel_scale = ACCEL_SCALE_2G; // Mặc định ±2g
static float gyro_scale = GYRO_SCALE_250;  // Mặc định ±250°/s

/**
 * @brief Ghi 1 byte vào thanh ghi MPU6050 qua I2C
 * @param reg_addr Địa chỉ thanh ghi
 * @param data Giá trị cần ghi
 * @return ESP_OK nếu thành công
 */
static esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data)
{
    // Tạo command: [địa chỉ thanh ghi, dữ liệu]
    uint8_t write_buf[2] = {reg_addr, data};

    // Gửi qua I2C
    return i2c_master_write_to_device(
        I2C_MASTER_NUM,                            // I2C port number
        MPU6050_ADDR,                              // Địa chỉ thiết bị
        write_buf,                                 // Buffer dữ liệu
        sizeof(write_buf),                         // Kích thước buffer
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS // Timeout
    );
}

/**
 * @brief Đọc nhiều byte từ MPU6050 qua I2C
 * @param reg_addr Địa chỉ thanh ghi bắt đầu đọc
 * @param data Buffer lưu dữ liệu đọc được
 * @param len Số byte cần đọc
 * @return ESP_OK nếu thành công
 */
static esp_err_t mpu6050_read_bytes(uint8_t reg_addr, uint8_t *data, size_t len)
{
    // Ghi địa chỉ thanh ghi cần đọc
    esp_err_t ret = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        &reg_addr,
        1,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (ret != ESP_OK)
    {
        return ret;
    }

    // Đọc dữ liệu
    return i2c_master_read_from_device(
        I2C_MASTER_NUM,
        MPU6050_ADDR,
        data,
        len,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Đọc 1 byte từ MPU6050
 */
static esp_err_t mpu6050_read_byte(uint8_t reg_addr, uint8_t *data)
{
    return mpu6050_read_bytes(reg_addr, data, 1);
}

/**
 * @brief Đọc 2 byte (1 word) từ MPU6050 và chuyển về little-endian
 */
static int16_t mpu6050_read_word(uint8_t reg_addr)
{
    uint8_t buf[2];
    esp_err_t ret = mpu6050_read_bytes(reg_addr, buf, 2);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read word from reg 0x%02x", reg_addr);
        return 0;
    }

    // MPU6050 lưu dữ liệu dạng big-endian (cao byte trước)
    return (int16_t)((buf[0] << 8) | buf[1]);
}

/**
 * @brief Khởi tạo I2C cho MPU6050
 */
static esp_err_t i2c_master_init(void)
{
    // Cấu hình I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Bật pull-up nội
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // Cài đặt cấu hình
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK)
    {
        return err;
    }

    // Khởi tạo I2C driver
    return i2c_driver_install(
        I2C_MASTER_NUM,
        conf.mode,
        I2C_MASTER_RX_BUF_DISABLE,
        I2C_MASTER_TX_BUF_DISABLE,
        0);
}

bool mpu6050_init(void)
{
    ESP_LOGI(TAG, "Initializing MPU6050...");

    // 1. Khởi tạo I2C
    esp_err_t ret = i2c_master_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // 2. Kiểm tra thiết bị có tồn tại không
    uint8_t who_am_i = 0;
    ret = mpu6050_read_byte(MPU6050_REG_WHO_AM_I, &who_am_i);

    if (ret != ESP_OK || who_am_i != 0x68)
    {
        ESP_LOGE(TAG, "MPU6050 not found! WHO_AM_I=0x%02x", who_am_i);
        return false;
    }

    ESP_LOGI(TAG, "MPU6050 found! WHO_AM_I=0x%02x", who_am_i);

    // 3. Reset thiết bị
    ret = mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x80); // BIT7 = reset
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Reset failed");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Đợi reset hoàn tất

    // 4. Cấu hình clock source (PLL với X axis gyro)
    ret = mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, 0x01);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Set clock source failed");
        return false;
    }

    // 5. Cấu hình sample rate (1kHz / (1 + SMPLRT_DIV))
    // SMPLRT_DIV = 19 => sample rate = 1kHz/(19+1) = 50Hz
    ret = mpu6050_write_byte(MPU6050_REG_SMPLRT_DIV, 19);

    // 6. Cấu hình DLPF (Digital Low Pass Filter) - cutoff 44Hz
    ret = mpu6050_write_byte(MPU6050_REG_CONFIG, 0x03); // DLPF_CFG = 3

    // 7. Cấu hình thang đo: ±8g cho gia tốc, ±500°/s cho con quay
    mpu6050_set_accel_scale(2); // ±8g
    mpu6050_set_gyro_scale(1);  // ±500°/s

    mpu6050_initialized = true;
    ESP_LOGI(TAG, "MPU6050 initialized successfully");

    return true;
}

bool mpu6050_set_accel_scale(uint8_t scale)
{
    uint8_t value = 0;

    // Đọc giá trị hiện tại của thanh ghi cấu hình gia tốc
    esp_err_t ret = mpu6050_read_byte(MPU6050_REG_ACCEL_CONFIG, &value);
    if (ret != ESP_OK)
    {
        return false;
    }

    // Xóa 3 bit AFS_SEL (bit4-3)
    value &= ~(0x18); // 0x18 = 00011000

    // Set scale mới
    value |= (scale << 3) & 0x18;

    // Ghi lại
    ret = mpu6050_write_byte(MPU6050_REG_ACCEL_CONFIG, value);
    if (ret != ESP_OK)
    {
        return false;
    }

    // Cập nhật scale factor
    switch (scale)
    {
    case 0:
        accel_scale = ACCEL_SCALE_2G;
        break;
    case 1:
        accel_scale = ACCEL_SCALE_4G;
        break;
    case 2:
        accel_scale = ACCEL_SCALE_8G;
        break;
    case 3:
        accel_scale = ACCEL_SCALE_16G;
        break;
    default:
        accel_scale = ACCEL_SCALE_2G;
    }

    return true;
}

bool mpu6050_set_gyro_scale(uint8_t scale)
{
    uint8_t value = 0;

    // Đọc giá trị hiện tại
    esp_err_t ret = mpu6050_read_byte(MPU6050_REG_GYRO_CONFIG, &value);
    if (ret != ESP_OK)
    {
        return false;
    }

    // Xóa 3 bit FS_SEL (bit4-3)
    value &= ~(0x18); // 0x18 = 00011000

    // Set scale mới
    value |= (scale << 3) & 0x18;

    // Ghi lại
    ret = mpu6050_write_byte(MPU6050_REG_GYRO_CONFIG, value);
    if (ret != ESP_OK)
    {
        return false;
    }

    // Cập nhật scale factor
    switch (scale)
    {
    case 0:
        gyro_scale = GYRO_SCALE_250;
        break;
    case 1:
        gyro_scale = GYRO_SCALE_500;
        break;
    case 2:
        gyro_scale = GYRO_SCALE_1000;
        break;
    case 3:
        gyro_scale = GYRO_SCALE_2000;
        break;
    default:
        gyro_scale = GYRO_SCALE_250;
    }

    return true;
}

bool mpu6050_read(mpu6050_data_t *data)
{
    if (!mpu6050_initialized || data == NULL)
    {
        return false;
    }

    uint8_t raw_data[14]; // 7 thanh ghi * 2 byte
    esp_err_t ret = mpu6050_read_bytes(MPU6050_REG_ACCEL_XOUT_H, raw_data, 14);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read sensor data");
        return false;
    }

    // Chuyển đổi dữ liệu từ big-endian sang giá trị thực
    int16_t ax_raw = (int16_t)((raw_data[0] << 8) | raw_data[1]);
    int16_t ay_raw = (int16_t)((raw_data[2] << 8) | raw_data[3]);
    int16_t az_raw = (int16_t)((raw_data[4] << 8) | raw_data[5]);
    int16_t temp_raw = (int16_t)((raw_data[6] << 8) | raw_data[7]);
    int16_t gx_raw = (int16_t)((raw_data[8] << 8) | raw_data[9]);
    int16_t gy_raw = (int16_t)((raw_data[10] << 8) | raw_data[11]);
    int16_t gz_raw = (int16_t)((raw_data[12] << 8) | raw_data[13]);

    // Chuyển đổi sang đơn vị vật lý
    // Gia tốc: raw / scale factor (LSB/g)
    data->ax = (float)ax_raw / accel_scale;
    data->ay = (float)ay_raw / accel_scale;
    data->az = (float)az_raw / accel_scale;

    // Tốc độ góc: raw / scale factor (LSB/°/s)
    data->gx = (float)gx_raw / gyro_scale;
    data->gy = (float)gy_raw / gyro_scale;
    data->gz = (float)gz_raw / gyro_scale;

    // Nhiệt độ: (raw / TEMP_SCALE) + TEMP_OFFSET
    data->temperature = ((float)temp_raw / TEMP_SCALE) + TEMP_OFFSET;

    // Thời gian lấy mẫu (ms kể từ khi khởi động)
    data->timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    return true;
}

bool mpu6050_is_available(void)
{
    uint8_t who_am_i = 0;
    esp_err_t ret = mpu6050_read_byte(MPU6050_REG_WHO_AM_I, &who_am_i);

    return (ret == ESP_OK && who_am_i == 0x68);
}

void mpu6050_sleep(void)
{
    uint8_t pwr_mgmt = 0;
    mpu6050_read_byte(MPU6050_REG_PWR_MGMT_1, &pwr_mgmt);
    pwr_mgmt |= 0x40; // BIT6 = sleep
    mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, pwr_mgmt);
    ESP_LOGI(TAG, "MPU6050 entering sleep mode");
}

void mpu6050_wakeup(void)
{
    uint8_t pwr_mgmt = 0;
    mpu6050_read_byte(MPU6050_REG_PWR_MGMT_1, &pwr_mgmt);
    pwr_mgmt &= ~0x40; // Clear sleep bit
    mpu6050_write_byte(MPU6050_REG_PWR_MGMT_1, pwr_mgmt);
    ESP_LOGI(TAG, "MPU6050 wake up");
}