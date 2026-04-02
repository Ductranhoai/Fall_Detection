#include "mpu_manager.h"
#include "fall_detection.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MPU_MANAGER";
static mpu_config_t s_mpu_config;
static bool s_initialized = false;
static TaskHandle_t s_read_task_handle = NULL;
static mpu_data_callback_t s_data_callback = NULL;
static uint32_t s_sample_interval_ms = 0;

// Forward declarations
static void continuous_read_task(void *arg);

// Default callback
static void default_callback(mpu6050_data_t *data)
{
    static uint32_t last_log = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Process fall detection
    fall_detection_process(data);
    fall_result_t result = fall_detection_get_result();
    
    // Log fall events
    if (result.fall_detected) {
        ESP_LOGW(TAG, "⚠️ FALL DETECTED! ⚠️");
        ESP_LOGW(TAG, "  Reason: %s", result.detection_reason);
        ESP_LOGW(TAG, "  Max impact: %.2fg", result.max_accel);
        ESP_LOGW(TAG, "  Final tilt: %.1f°", result.final_tilt);
    }
    
    // Normal logging every 2 seconds
    if (now - last_log > 2000) {
        ESP_LOGI(TAG, "Status - State: %s, Pitch: %.1f°, Roll: %.1f°, Z: %.2fg",
                 fall_state_to_string(result.state),
                 data->pitch, data->roll, data->accel_z);
        last_log = now;
    }
}

// Continuous read task
static void continuous_read_task(void *arg)
{
    mpu6050_data_t data;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        if (mpu6050_read(&data) == ESP_OK) {
            mpu6050_calc_angles(&data);
            
            if (s_data_callback) {
                s_data_callback(&data);
            }
        }
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(s_sample_interval_ms));
    }
}

// Initialize MPU with custom config
esp_err_t mpu_manager_init(const mpu_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "MPU already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Copy configuration
    memcpy(&s_mpu_config, config, sizeof(mpu_config_t));
    
    // Prepare MPU6050 config
    mpu6050_config_t mpu_config = mpu6050_get_default_config();
    mpu_config.i2c_port = s_mpu_config.i2c_port;
    mpu_config.i2c_freq_hz = s_mpu_config.i2c_freq;
    mpu_config.sda_pin = s_mpu_config.sda_pin;
    mpu_config.scl_pin = s_mpu_config.scl_pin;
    mpu_config.accel_range = s_mpu_config.accel_range;
    mpu_config.gyro_range = s_mpu_config.gyro_range;
    mpu_config.dlpf_bandwidth = s_mpu_config.dlpf_bandwidth;
    mpu_config.sample_rate_hz = s_mpu_config.sample_rate_hz;
    mpu_config.enable_fifo = s_mpu_config.enable_fifo;
    
    // Initialize MPU6050
    esp_err_t ret = mpu6050_init(&mpu_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MPU6050");
        return ret;
    }
    
    // Calibrate if enabled
    if (s_mpu_config.enable_calibration) {
        vTaskDelay(pdMS_TO_TICKS(500));
        mpu6050_calibrate();
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "MPU Manager initialized successfully");

    fall_config_t fall_config = fall_get_default_config();
    fall_detection_init(&fall_config);
    
    return ESP_OK;
}

// Initialize with default config
esp_err_t mpu_manager_init_default(void)
{
    mpu_config_t config = mpu_get_default_config();
    return mpu_manager_init(&config);
}

// Deinitialize
esp_err_t mpu_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    mpu_manager_stop_monitoring();
    mpu6050_deinit();
    
    s_initialized = false;
    ESP_LOGI(TAG, "MPU Manager deinitialized");
    
    return ESP_OK;
}

// Get data
esp_err_t mpu_manager_get_data(mpu6050_data_t *data)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = mpu6050_read(data);
    if (ret == ESP_OK) {
        mpu6050_calc_angles(data);
    }
    
    return ret;
}

// Start monitoring
void mpu_manager_start_monitoring(mpu_data_callback_t callback)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "MPU not initialized");
        return;
    }
    
    if (s_read_task_handle != NULL) {
        ESP_LOGW(TAG, "Monitoring already running");
        return;
    }
    
    s_data_callback = callback ? callback : default_callback;
    s_sample_interval_ms = s_mpu_config.read_interval_ms;
    
    xTaskCreate(continuous_read_task, "mpu_read", 4096, NULL, 5, &s_read_task_handle);
    ESP_LOGI(TAG, "Started monitoring at %d ms interval", s_sample_interval_ms);
}

// Stop monitoring
void mpu_manager_stop_monitoring(void)
{
    if (s_read_task_handle) {
        vTaskDelete(s_read_task_handle);
        s_read_task_handle = NULL;
        s_data_callback = NULL;
        ESP_LOGI(TAG, "Stopped monitoring");
    }
}

// Get config
const mpu_config_t* mpu_manager_get_config(void)
{
    return &s_mpu_config;
}

// Check if initialized
bool mpu_manager_is_initialized(void)
{
    return s_initialized;
}

// Check if monitoring
bool mpu_manager_is_monitoring(void)
{
    return (s_read_task_handle != NULL);
}