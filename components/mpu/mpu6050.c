#include "mpu6050.h"           // Header định nghĩa hằng số, struct, enum của MPU6050
#include "esp_log.h"           // Logging ESP-IDF (ESP_LOGI, ESP_LOGE, ...)
#include "esp_timer.h"         // Timer ESP-IDF, lấy timestamp chính xác
#include "freertos/FreeRTOS.h" // FreeRTOS: Task, Queue
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>   // Hàm toán học: sqrt(), atan2()
#include <string.h> // Hàm xử lý string: memcpy(), memset()

static const char *TAG = "MPU6050"; // Tag dùng để log

// =======================================
// Biến lưu trạng thái và cấu hình
// =======================================
static i2c_port_t s_i2c_port = I2C_NUM_0;          // Cổng I2C dùng
static bool s_initialized = false;                 // Flag sensor đã init chưa
static mpu6050_config_t s_config;                  // Lưu cấu hình sensor
static mpu_data_callback_t s_data_callback = NULL; // Callback khi đọc liên tục
static TaskHandle_t s_read_task_handle = NULL;     // Handle task đọc liên tục
static uint32_t s_sample_interval_ms = 0;          // Khoảng thời gian giữa 2 lần đọc liên tục

// Scale factor: chuyển dữ liệu raw sang giá trị vật lý
static float s_accel_scale = 16384.0; // ±2g → 1g = 16384
static float s_gyro_scale = 131.0;    // ±250°/s → 1°/s = 131

// Bias để hiệu chuẩn
static float s_gyro_bias[3] = {0, 0, 0};  // gx, gy, gz
static float s_accel_bias[3] = {0, 0, 0}; // ax, ay, az

// =======================================
// Khai báo các hàm private
// =======================================
static esp_err_t write_byte(uint8_t reg, uint8_t data);
static esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len);
static esp_err_t set_accel_range(mpu_accel_range_t range);
static esp_err_t set_gyro_range(mpu_gyro_range_t range);
static esp_err_t set_dlpf(mpu_dlpf_bandwidth_t bandwidth);
static esp_err_t set_sample_rate(uint32_t rate_hz);
static void update_scales(void);

// =======================================
// Giao tiếp I2C
// =======================================

// Ghi 1 byte vào register của MPU6050
static esp_err_t write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data}; // Buffer: [địa chỉ register, dữ liệu]
    return i2c_master_write_to_device(s_i2c_port, MPU6050_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

// Đọc nhiều byte từ register
static esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    // Gửi 1 byte register, đọc len byte trả về
    return i2c_master_write_read_device(s_i2c_port, MPU6050_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

// =======================================
// Cấu hình sensor
// =======================================

// Thiết lập phạm vi accelerometer (±2g, ±4g, ±8g, ±16g)
static esp_err_t set_accel_range(mpu_accel_range_t range)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_ACCEL_CONFIG, &config, 1);
    if (ret != ESP_OK)
        return ret;

    config &= ~0x18; // Xóa bit AFS_SEL (bit 3,4)
    config |= range; // Set phạm vi mới

    ret = write_byte(MPU6050_ACCEL_CONFIG, config);
    if (ret != ESP_OK)
        return ret;

    // Cập nhật hệ số scale
    switch (range)
    {
    case MPU_ACCEL_RANGE_2G:
        s_accel_scale = 16384.0;
        break;
    case MPU_ACCEL_RANGE_4G:
        s_accel_scale = 8192.0;
        break;
    case MPU_ACCEL_RANGE_8G:
        s_accel_scale = 4096.0;
        break;
    case MPU_ACCEL_RANGE_16G:
        s_accel_scale = 2048.0;
        break;
    }

    return ESP_OK;
}

// Thiết lập phạm vi gyroscope (±250, ±500, ±1000, ±2000 °/s)
static esp_err_t set_gyro_range(mpu_gyro_range_t range)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_GYRO_CONFIG, &config, 1);
    if (ret != ESP_OK)
        return ret;

    config &= ~0x18; // Xóa bit FS_SEL (bit 3,4)
    config |= range; // Set phạm vi mới

    ret = write_byte(MPU6050_GYRO_CONFIG, config);
    if (ret != ESP_OK)
        return ret;

    // Cập nhật hệ số scale
    switch (range)
    {
    case MPU_GYRO_RANGE_250:
        s_gyro_scale = 131.0;
        break;
    case MPU_GYRO_RANGE_500:
        s_gyro_scale = 65.5;
        break;
    case MPU_GYRO_RANGE_1000:
        s_gyro_scale = 32.8;
        break;
    case MPU_GYRO_RANGE_2000:
        s_gyro_scale = 16.4;
        break;
    }

    return ESP_OK;
}

// Thiết lập DLPF (Low-pass filter) để lọc nhiễu
static esp_err_t set_dlpf(mpu_dlpf_bandwidth_t bandwidth)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_CONFIG, &config, 1);
    if (ret != ESP_OK)
        return ret;

    config &= ~0x07;     // Xóa bit DLPF_CFG (bit 0–2)
    config |= bandwidth; // Set băng thông lọc

    return write_byte(MPU6050_CONFIG, config);
}

// Thiết lập tần số lấy mẫu (sample rate)
static esp_err_t set_sample_rate(uint32_t rate_hz)
{
    // Công thức:
    // sample_rate = gyro_output_rate / (1 + SMPLRT_DIV)
    // gyro_output_rate = 8kHz khi DLPF enabled
    uint8_t divider = (8000 / rate_hz) - 1;
    if (divider > 255)
        divider = 255;
    return write_byte(0x19, divider); // SMPLRT_DIV register
}

// Cập nhật hệ số scale dựa trên config hiện tại
static void update_scales(void)
{
    set_accel_range(s_config.accel_range);
    set_gyro_range(s_config.gyro_range);
}

// =======================================
// Cấu hình mặc định
// =======================================
mpu6050_config_t mpu6050_get_default_config(void)
{
    mpu6050_config_t config = {
        .i2c_port = I2C_NUM_0,
        .i2c_freq_hz = 400000,
        .sda_pin = 21,
        .scl_pin = 22,
        .accel_range = MPU_ACCEL_RANGE_2G,
        .gyro_range = MPU_GYRO_RANGE_250,
        .dlpf_bandwidth = MPU_DLPF_BW_21,
        .sample_rate_hz = 100,
        .enable_fifo = false,
        .fifo_buffer_size = 1024};
    return config;
}

// =======================================
// Khởi tạo sensor
// =======================================
esp_err_t mpu6050_init(const mpu6050_config_t *config)
{
    if (s_initialized)
    {
        ESP_LOGW(TAG, "MPU6050 already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&s_config, config, sizeof(mpu6050_config_t));
    s_i2c_port = s_config.i2c_port;

    // Cấu hình I2C
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = s_config.sda_pin,
        .scl_io_num = s_config.scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = s_config.i2c_freq_hz,
    };
    esp_err_t ret = i2c_param_config(s_config.i2c_port, &i2c_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2c_driver_install(s_config.i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wake-up MPU6050
    write_byte(MPU6050_PWR_MGMT_1, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Kiểm tra WHO_AM_I = 0x68
    uint8_t whoami;
    ret = read_bytes(MPU6050_WHO_AM_I, &whoami, 1);
    if (ret != ESP_OK || whoami != 0x68)
    {
        ESP_LOGE(TAG, "Wrong device ID: 0x%02X", whoami);
        return ESP_ERR_NOT_FOUND;
    }

    // Cấu hình sensor
    set_accel_range(s_config.accel_range);
    set_gyro_range(s_config.gyro_range);
    set_dlpf(s_config.dlpf_bandwidth);
    set_sample_rate(s_config.sample_rate_hz);

    // FIFO (nếu cần)
    if (s_config.enable_fifo)
    {
        uint8_t fifo_config = 0;
        read_bytes(0x23, &fifo_config, 1);
        fifo_config |= (1 << 6); // Enable FIFO
        write_byte(0x23, fifo_config);
        write_byte(0x6A, 0x40); // Reset FIFO
        vTaskDelay(pdMS_TO_TICKS(50));
        write_byte(0x6A, 0x00);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "MPU6050 initialized successfully!");
    return ESP_OK;
}

// =======================================
// Đọc dữ liệu sensor
// =======================================
esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;

    uint8_t buf[14];
    // ACCEL_XOUT_H = 0x3B
    esp_err_t ret = read_bytes(MPU6050_ACCEL_XOUT_H, buf, 14);
    if (ret != ESP_OK)
        return ret;

    // Parse raw data (2 byte mỗi trục)
    data->ax = (buf[0] << 8) | buf[1];
    data->ay = (buf[2] << 8) | buf[3];
    data->az = (buf[4] << 8) | buf[5];

    int16_t temp_raw = (buf[6] << 8) | buf[7];
    data->temperature = temp_raw / 340.0 + 36.53; // Công thức datasheet

    data->gx = (buf[8] << 8) | buf[9];
    data->gy = (buf[10] << 8) | buf[11];
    data->gz = (buf[12] << 8) | buf[13];

    // Trừ bias
    float ax_comp = data->ax - s_accel_bias[0];
    float ay_comp = data->ay - s_accel_bias[1];
    float az_comp = data->az - s_accel_bias[2];

    float gx_comp = data->gx - s_gyro_bias[0];
    float gy_comp = data->gy - s_gyro_bias[1];
    float gz_comp = data->gz - s_gyro_bias[2];

    // Chuyển raw -> g / deg/s
    data->accel_x = ax_comp / s_accel_scale;
    data->accel_y = ay_comp / s_accel_scale;
    data->accel_z = az_comp / s_accel_scale;

    data->gyro_x = gx_comp / s_gyro_scale;
    data->gyro_y = gy_comp / s_gyro_scale;
    data->gyro_z = gz_comp / s_gyro_scale;

    data->timestamp_ms = esp_timer_get_time() / 1000; // Timestamp ms
    return ESP_OK;
}

// =======================================
// Tính góc pitch & roll
// =======================================
esp_err_t mpu6050_calc_angles(mpu6050_data_t *data)
{
    if (!data)
        return ESP_ERR_INVALID_ARG;

    // Pitch: xoay quanh X
    // Công thức: pitch = atan2(-ax, sqrt(ay^2 + az^2)) * 180/PI
    data->pitch = atan2(-data->accel_x, sqrt(data->accel_y * data->accel_y + data->accel_z * data->accel_z)) * 180.0 / M_PI;

    // Roll: xoay quanh Y
    data->roll = atan2(data->accel_y, data->accel_z) * 180.0 / M_PI;

    return ESP_OK;
}

// =======================================
// Hiệu chuẩn sensor (calibration)
// =======================================
esp_err_t mpu6050_calibrate(void)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Bắt đầu calibration. Giữ sensor yên!");
    vTaskDelay(pdMS_TO_TICKS(100));

    const int num_samples = 200;
    int32_t sum_accel[3] = {0, 0, 0};
    int32_t sum_gyro[3] = {0, 0, 0};

    for (int i = 0; i < num_samples; i++)
    {
        mpu6050_data_t data;
        if (mpu6050_read(&data) == ESP_OK)
        {
            sum_accel[0] += data.ax;
            sum_accel[1] += data.ay;
            sum_accel[2] += data.az;
            sum_gyro[0] += data.gx;
            sum_gyro[1] += data.gy;
            sum_gyro[2] += data.gz;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    for (int i = 0; i < 3; i++)
    {
        s_accel_bias[i] = sum_accel[i] / num_samples;
        s_gyro_bias[i] = sum_gyro[i] / num_samples;
    }
    // Điều chỉnh trục Z accelerometer cho 1g
    float expected_z = s_accel_scale; // ±2g -> 1g = 16384
    s_accel_bias[2] -= expected_z;

    ESP_LOGI(TAG, "Calibration xong!");
    ESP_LOGI(TAG, "Accel bias: X=%d Y=%d Z=%d", (int)s_accel_bias[0], (int)s_accel_bias[1], (int)s_accel_bias[2]);
    ESP_LOGI(TAG, "Gyro bias: X=%d Y=%d Z=%d", (int)s_gyro_bias[0], (int)s_gyro_bias[1], (int)s_gyro_bias[2]);

    return ESP_OK;
}

// =======================================
// Callback và đọc liên tục
// =======================================
void mpu6050_set_data_callback(mpu_data_callback_t callback)
{
    s_data_callback = callback;
}

// Task đọc liên tục
static void continuous_read_task(void *arg)
{
    mpu6050_data_t data;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1)
    {
        if (mpu6050_read(&data) == ESP_OK)
        {
            mpu6050_calc_angles(&data);
            if (s_data_callback)
                s_data_callback(&data);
        }
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(s_sample_interval_ms));
    }
}

// Start/Stop đọc liên tục
void mpu6050_start_continuous_read(uint32_t interval_ms)
{
    if (s_read_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Đọc liên tục đang chạy");
        return;
    }
    s_sample_interval_ms = interval_ms;
    xTaskCreate(continuous_read_task, "mpu_read", 4096, NULL, 5, &s_read_task_handle);
    ESP_LOGI(TAG, "Bắt đầu đọc liên tục mỗi %d ms", interval_ms);
}

void mpu6050_stop_continuous_read(void)
{
    if (s_read_task_handle)
    {
        vTaskDelete(s_read_task_handle);
        s_read_task_handle = NULL;
        ESP_LOGI(TAG, "Dừng đọc liên tục");
    }
}

// =======================================
// Deinit và kiểm tra
// =======================================
esp_err_t mpu6050_deinit(void)
{
    if (!s_initialized)
        return ESP_OK;
    mpu6050_stop_continuous_read();
    write_byte(MPU6050_PWR_MGMT_1, 0x40); // Sleep
    esp_err_t ret = i2c_driver_delete(s_i2c_port);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Xóa driver I2C thất bại");
    s_initialized = false;
    ESP_LOGI(TAG, "MPU6050 deinit");
    return ret;
}

bool mpu6050_is_initialized(void)
{
    return s_initialized;
}