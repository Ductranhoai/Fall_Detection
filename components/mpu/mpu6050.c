#include "mpu6050.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>

static const char *TAG = "MPU6050";

// Static variables
static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static mpu6050_config_t s_config;
static mpu_data_callback_t s_data_callback = NULL;
static TaskHandle_t s_read_task_handle = NULL;
static uint32_t s_sample_interval_ms = 0;
static float s_accel_scale = 16384.0; // Default for ±2g
static float s_gyro_scale = 131.0;    // Default for ±250 deg/s
static float s_gyro_bias[3] = {0, 0, 0};
static float s_accel_bias[3] = {0, 0, 0};

// Forward declarations
static esp_err_t write_byte(uint8_t reg, uint8_t data);
static esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len);
static esp_err_t set_accel_range(mpu_accel_range_t range);
static esp_err_t set_gyro_range(mpu_gyro_range_t range);
static esp_err_t set_dlpf(mpu_dlpf_bandwidth_t bandwidth);
static esp_err_t set_sample_rate(uint32_t rate_hz);
static void update_scales(void);

// I2C operations
static esp_err_t write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(s_i2c_port, MPU6050_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(s_i2c_port, MPU6050_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

// Configuration functions
static esp_err_t set_accel_range(mpu_accel_range_t range)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_ACCEL_CONFIG, &config, 1);
    if (ret != ESP_OK)
        return ret;

    config &= ~0x18; // Clear AFS_SEL bits
    config |= range;

    ret = write_byte(MPU6050_ACCEL_CONFIG, config);
    if (ret != ESP_OK)
        return ret;

    // Update scale factor based on range
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

static esp_err_t set_gyro_range(mpu_gyro_range_t range)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_GYRO_CONFIG, &config, 1);
    if (ret != ESP_OK)
        return ret;

    config &= ~0x18; // Clear FS_SEL bits
    config |= range;

    ret = write_byte(MPU6050_GYRO_CONFIG, config);
    if (ret != ESP_OK)
        return ret;

    // Update scale factor based on range
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

static esp_err_t set_dlpf(mpu_dlpf_bandwidth_t bandwidth)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_CONFIG, &config, 1);
    if (ret != ESP_OK)
        return ret;

    config &= ~0x07; // Clear DLPF_CFG bits
    config |= bandwidth;

    return write_byte(MPU6050_CONFIG, config);
}

static esp_err_t set_sample_rate(uint32_t rate_hz)
{
    // Sample rate = Gyroscope output rate / (1 + SMPLRT_DIV)
    // Gyroscope output rate = 8kHz when DLPF enabled, 1kHz when disabled
    // For simplicity, assume DLPF enabled -> 8kHz internal rate
    uint8_t divider = (8000 / rate_hz) - 1;
    if (divider > 255)
        divider = 255;

    return write_byte(0x19, divider); // SMPLRT_DIV register
}

static void update_scales(void)
{
    // Update based on current config
    set_accel_range(s_config.accel_range);
    set_gyro_range(s_config.gyro_range);
}

// Public: Get default configuration
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

// Public: Initialize MPU6050
esp_err_t mpu6050_init(const mpu6050_config_t *config)
{
    if (s_initialized)
    {
        ESP_LOGW(TAG, "MPU6050 already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Copy configuration
    memcpy(&s_config, config, sizeof(mpu6050_config_t));
    s_i2c_port = s_config.i2c_port;

    ESP_LOGI(TAG, "Initializing MPU6050...");
    ESP_LOGI(TAG, "I2C: port=%d, SDA=%d, SCL=%d, freq=%dHz",
             s_config.i2c_port, s_config.sda_pin, s_config.scl_pin, s_config.i2c_freq_hz);

    // Configure I2C if not already configured
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

    // Wake up MPU6050
    ret = write_byte(MPU6050_PWR_MGMT_1, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to wake up MPU6050");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // Check WHO_AM_I
    uint8_t whoami;
    ret = read_bytes(MPU6050_WHO_AM_I, &whoami, 1);
    if (ret != ESP_OK || whoami != 0x68)
    {
        ESP_LOGE(TAG, "Wrong device ID: 0x%02X (expected 0x68)", whoami);
        return ESP_ERR_NOT_FOUND;
    }

    // Configure sensor
    set_accel_range(s_config.accel_range);
    set_gyro_range(s_config.gyro_range);
    set_dlpf(s_config.dlpf_bandwidth);
    set_sample_rate(s_config.sample_rate_hz);

    // Optional: Enable FIFO if needed
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

// Public: Deinitialize MPU6050
esp_err_t mpu6050_deinit(void)
{
    if (!s_initialized)
    {
        return ESP_OK;
    }

    mpu6050_stop_continuous_read();

    // Put MPU6050 to sleep
    write_byte(MPU6050_PWR_MGMT_1, 0x40);

    esp_err_t ret = i2c_driver_delete(s_i2c_port);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete I2C driver");
    }

    s_initialized = false;
    ESP_LOGI(TAG, "MPU6050 deinitialized");
    return ret;
}

// Public: Read sensor data
esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[14];

    // Read 14 bytes from ACCEL_XOUT_H
    esp_err_t ret = read_bytes(MPU6050_ACCEL_XOUT_H, buf, 14);
    if (ret != ESP_OK)
        return ret;

    // Parse accelerometer (2 bytes each)
    data->ax = (buf[0] << 8) | buf[1];
    data->ay = (buf[2] << 8) | buf[3];
    data->az = (buf[4] << 8) | buf[5];

    // Parse temperature
    int16_t temp_raw = (buf[6] << 8) | buf[7];
    data->temperature = temp_raw / 340.0 + 36.53;

    // Parse gyroscope (2 bytes each)
    data->gx = (buf[8] << 8) | buf[9];
    data->gy = (buf[10] << 8) | buf[11];
    data->gz = (buf[12] << 8) | buf[13];

    // Apply bias compensation
    float ax_comp = data->ax - s_accel_bias[0];
    float ay_comp = data->ay - s_accel_bias[1];
    float az_comp = data->az - s_accel_bias[2];

    float gx_comp = data->gx - s_gyro_bias[0];
    float gy_comp = data->gy - s_gyro_bias[1];
    float gz_comp = data->gz - s_gyro_bias[2];

    // Convert to physical units
    data->accel_x = ax_comp / s_accel_scale;
    data->accel_y = ay_comp / s_accel_scale;
    data->accel_z = az_comp / s_accel_scale;

    data->gyro_x = gx_comp / s_gyro_scale;
    data->gyro_y = gy_comp / s_gyro_scale;
    data->gyro_z = gz_comp / s_gyro_scale;

    // Get timestamp
    data->timestamp_ms = esp_timer_get_time() / 1000;

    return ESP_OK;
}

// Public: Calculate pitch and roll
esp_err_t mpu6050_calc_angles(mpu6050_data_t *data)
{
    if (!data)
        return ESP_ERR_INVALID_ARG;

    // Pitch: rotation around X axis
    data->pitch = atan2(-data->accel_x,
                        sqrt(data->accel_y * data->accel_y +
                             data->accel_z * data->accel_z)) *
                  180.0 / M_PI;

    // Roll: rotation around Y axis
    data->roll = atan2(data->accel_y, data->accel_z) * 180.0 / M_PI;

    return ESP_OK;
}

// Public: Calibrate sensors (remove bias)
esp_err_t mpu6050_calibrate(void)
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting calibration... Keep sensor still!");
    vTaskDelay(pdMS_TO_TICKS(100));

    // Calibration samples
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

    // Calculate biases
    for (int i = 0; i < 3; i++)
    {
        s_accel_bias[i] = sum_accel[i] / num_samples;
        s_gyro_bias[i] = sum_gyro[i] / num_samples;
    }

    // For accelerometer, we assume Z axis should read 1g when still
    // So we need to adjust Z bias accordingly
    float expected_z = s_accel_scale; // 16384 for ±2g range
    s_accel_bias[2] -= expected_z;

    ESP_LOGI(TAG, "Calibration complete!");
    ESP_LOGI(TAG, "Accel bias: X=%d, Y=%d, Z=%d",
             (int)s_accel_bias[0], (int)s_accel_bias[1], (int)s_accel_bias[2]);
    ESP_LOGI(TAG, "Gyro bias: X=%d, Y=%d, Z=%d",
             (int)s_gyro_bias[0], (int)s_gyro_bias[1], (int)s_gyro_bias[2]);

    return ESP_OK;
}

// Public: Set data ready callback
void mpu6050_set_data_callback(mpu_data_callback_t callback)
{
    s_data_callback = callback;
}

// Continuous read task
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
            {
                s_data_callback(&data);
            }
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(s_sample_interval_ms));
    }
}

// Public: Start continuous reading
void mpu6050_start_continuous_read(uint32_t interval_ms)
{
    if (s_read_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Continuous read already running");
        return;
    }

    s_sample_interval_ms = interval_ms;

    xTaskCreate(continuous_read_task, "mpu_read", 4096, NULL, 5, &s_read_task_handle);
    ESP_LOGI(TAG, "Started continuous read at %d ms interval", interval_ms);
}

// Public: Stop continuous reading
void mpu6050_stop_continuous_read(void)
{
    if (s_read_task_handle)
    {
        vTaskDelete(s_read_task_handle);
        s_read_task_handle = NULL;
        ESP_LOGI(TAG, "Stopped continuous read");
    }
}

// Public: Check if initialized
bool mpu6050_is_initialized(void)
{
    return s_initialized;
}