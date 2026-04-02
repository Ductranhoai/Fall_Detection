// #include "cli.h"
// #include "fs.h"
// // #include "ble.h"

// #include "wifi_manager.h"
// #include "wifi_cli.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// static void cli_task(void *arg)
// {
//     cli_start();
// }

// void app_main(void)
// {
//     wifi_manager_init();
//     fs_init();
//     // ble_init();
//     cli_init();

//     cli_register_fs();
//     cli_register_mem();
//     cli_register_i2c();
//     wifi_cli_register();
//     cli_register_system();

//     xTaskCreate(cli_task, "cli", 4096, NULL, 5, NULL);
// }

// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "driver/i2c.h"

// #include "mpu6050.h"
// #include "cli.h"
// #include "fs.h"
// // #include "ble.h"
// #include "wifi_manager.h"
// #include "wifi_cli.h"

// static const char *TAG = "MAIN";

// // I2C config
// #define I2C_MASTER_SCL_IO    22
// #define I2C_MASTER_SDA_IO    21
// #define I2C_MASTER_NUM       I2C_NUM_0
// #define I2C_MASTER_FREQ_HZ   400000

// // ================= I2C INIT =================
// static void i2c_init(void)
// {
//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,
//         .sda_io_num = I2C_MASTER_SDA_IO,
//         .scl_io_num = I2C_MASTER_SCL_IO,
//         .sda_pullup_en = GPIO_PULLUP_ENABLE,
//         .scl_pullup_en = GPIO_PULLUP_ENABLE,
//         .master.clk_speed = I2C_MASTER_FREQ_HZ,
//     };

//     ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
//     ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER, 0, 0, 0));
//     ESP_LOGI(TAG, "I2C initialized");
// }

// // ================= MPU TASK =================
// static void mpu_task(void *arg)
// {
//     ESP_LOGI(TAG, "Initializing MPU6050...");

//     if (mpu6050_init(I2C_MASTER_NUM) != ESP_OK) {
//         ESP_LOGE(TAG, "MPU6050 init failed!");
//         vTaskDelete(NULL);
//         return;
//     }

//     mpu6050_data_t data;

//     while (1) {
//         if (mpu6050_read(&data) == ESP_OK) {
//             mpu6050_calc_angles(&data);
//             mpu6050_print(&data);
//         } else {
//             ESP_LOGE(TAG, "Read MPU6050 failed");
//         }

//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }

// // ================= CLI TASK =================
// static void cli_task(void *arg)
// {
//     cli_start();
// }

// // ================= MAIN =================
// void app_main(void)
// {
//     ESP_LOGI(TAG, "System starting...");

//     // Init subsystem
//     i2c_init();

//     wifi_manager_init();
//     fs_init();
//     // ble_init();

//     cli_init();
//     cli_register_fs();
//     cli_register_mem();
//     cli_register_i2c();
//     wifi_cli_register();
//     cli_register_system();

//     // Create tasks
//     xTaskCreate(cli_task, "cli", 4096, NULL, 5, NULL);
//     xTaskCreate(mpu_task, "mpu", 4096, NULL, 5, NULL);
// }
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "cli.h"
#include "fs.h"
#include "wifi_manager.h"
#include "wifi_cli.h"
#include "mpu_manager.h"

static const char *TAG = "MAIN";

// CLI task
static void cli_task(void *arg)
{
    cli_start();
}

// Optional: MPU data callback for custom processing
static void on_mpu_data(mpu6050_data_t *data)
{
    // This runs in MPU read task context
    // Add your custom logic here (e.g., fall detection)
    static uint32_t last_log = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Example: Detect potential fall
    if (abs(data->accel_z) < 0.5) {
        ESP_LOGW(TAG, "⚠️ Potential fall detected! Z-accel: %.2fg", data->accel_z);
        // Here you could trigger alarm, send notification, etc.
    }
    
    // Log every 2 seconds (optional)
    if (now - last_log > 2000) {
        ESP_LOGI(TAG, "MPU Status - Pitch: %.1f°, Roll: %.1f°, Z: %.2fg", 
                 data->pitch, data->roll, data->accel_z);
        last_log = now;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "System starting...");
    
    // Initialize WiFi
    wifi_manager_init();
    
    // Initialize filesystem
    fs_init();
    
    // Initialize MPU with default configuration
    // You can use different configs: mpu_get_default_config(), 
    // mpu_get_fast_config(), or mpu_get_lowpower_config()
    mpu_config_t mpu_config = mpu_get_default_config();
    
    esp_err_t ret = mpu_manager_init(&mpu_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MPU! Error: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "MPU initialized successfully");
        // Start monitoring with callback
        // Pass NULL to use default fall detection, or your custom callback
        mpu_manager_start_monitoring(on_mpu_data);
    }
    
    // Initialize CLI (will also register MPU commands)
    cli_init_all();
    
    // Create CLI task (already created in cli_init_all)
    // No need to create another CLI task here
    
    ESP_LOGI(TAG, "System ready! Use CLI commands:");
    ESP_LOGI(TAG, "  - mpu_read      : Read sensor once");
    ESP_LOGI(TAG, "  - mpu_cal       : Calibrate sensor");
    ESP_LOGI(TAG, "  - mpu_monitor   : Start/stop monitoring");
    ESP_LOGI(TAG, "  - mpu_config    : Show configuration");
    ESP_LOGI(TAG, "  - mpu_test      : Quick hardware test");
}