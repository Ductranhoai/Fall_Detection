
// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"

// #include "cli.h"
// #include "fs.h"
// #include "wifi_manager.h"
// #include "wifi_cli.h"
// #include "mpu_manager.h"

// static const char *TAG = "MAIN";

// // CLI task
// static void cli_task(void *arg)
// {
//     cli_start();
// }

// // Optional: MPU data callback for custom processing
// static void on_mpu_data(mpu6050_data_t *data)
// {
//     // This runs in MPU read task context
//     // Add your custom logic here (e.g., fall detection)
//     static uint32_t last_log = 0;
//     uint32_t now = esp_timer_get_time() / 1000;

//     // Example: Detect potential fall
//     if (abs(data->accel_z) < 0.5)
//     {
//         ESP_LOGW(TAG, " Potential fall detected! Z-accel: %.2fg", data->accel_z);
//         // Here you could trigger alarm, send notification, etc.
//     }

//     // Log every 2 seconds (optional)
//     if (now - last_log > 2000)
//     {
//         ESP_LOGI(TAG, "MPU Status - Pitch: %.1f°, Roll: %.1f°, Z: %.2fg",
//                  data->pitch, data->roll, data->accel_z);
//         last_log = now;
//     }
// }

// void app_main(void)
// {
//     ESP_LOGI(TAG, "System starting...");

//     // Initialize WiFi
//     wifi_manager_init();
//     vTaskDelay(pdMS_TO_TICKS(500));

//     // Initialize filesystem
//     fs_init();

//     // Initialize MPU with default configuration
//     // You can use different configs: mpu_get_default_config(),
//     // mpu_get_fast_config(), or mpu_get_lowpower_config()
//     mpu_config_t mpu_config = mpu_get_default_config();

//     esp_err_t ret = mpu_manager_init(&mpu_config);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to initialize MPU! Error: %s", esp_err_to_name(ret));
//     }
//     else
//     {
//         ESP_LOGI(TAG, "MPU initialized successfully");
//         // Start monitoring with callback
//         // Pass NULL to use default fall detection, or your custom callback
//         mpu_manager_start_monitoring(on_mpu_data);
//         ESP_LOGI(TAG, "✓ MPU monitoring started");
//     }

//     // Initialize CLI (will also register MPU commands)
//     cli_init_all();

//     // Create CLI task (already created in cli_init_all)
//     // No need to create another CLI task here

//     ESP_LOGI(TAG, "========================================");
//     ESP_LOGI(TAG, "System ready!");
//     ESP_LOGI(TAG, "========================================");
//     ESP_LOGI(TAG, "Available commands:");
//     ESP_LOGI(TAG, "  WiFi: wifi_scan, wifi_status, wifi_connect, ifconfig");
//     ESP_LOGI(TAG, "  MPU:  mpu_read, mpu_read -w, mpu_stop, mpu_cal, mpu_test");
//     ESP_LOGI(TAG, "  FS:   ls, pwd, cd, cat, touch");
//     ESP_LOGI(TAG, "  FALL:  fall_status, fall_test, fall_reset");
//     ESP_LOGI(TAG, "  System: reboot, free, tasks");
//     ESP_LOGI(TAG, "========================================");
// }




#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "cli.h"
#include "fs.h"
#include "wifi_manager.h"
#include "wifi_cli.h"
#include "mpu_manager.h"
#include "mpu_config.h"
#include "fall_detection.h"

static const char *TAG = "MAIN";

static void on_mpu_data(mpu6050_data_t *data)
{
    fall_detection_process(data);
    fall_result_t result = fall_detection_get_result();

    // Chỉ in khi té
    if (result.fall_detected)
    {
        printf("\n>>> NGUOI DUNG BI TE! <<<\n\n");
    }
}

void app_main(void)
{
    printf("\n========================================\n");
    printf("     FALL DETECTION SYSTEM\n");
    printf("========================================\n\n");

    // Init WiFi
    wifi_manager_init();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Init filesystem
    fs_init();

    // Init MPU
    mpu_config_t mpu_config = mpu_get_default_config();
    esp_err_t ret = mpu_manager_init(&mpu_config);

    if (ret != ESP_OK)
    {
        printf("[ERROR] MPU init failed!\n");
    }
    else
    {
        printf("[OK] MPU initialized\n");
        mpu_manager_start_monitoring(on_mpu_data);
        printf("[OK] Fall detection started\n");
    }

    // Init CLI
    cli_init_all();

    // Create CLI task (already created in cli_init_all)
    // No need to create another CLI task here

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Available commands:");
    ESP_LOGI(TAG, "  WiFi: wifi_scan, wifi_status, wifi_connect, ifconfig");
    ESP_LOGI(TAG, "  MPU:  mpu_read, mpu_read -w, mpu_stop, mpu_cal, mpu_test");
    ESP_LOGI(TAG, "  FS:   ls, pwd, cd, cat, touch");
    ESP_LOGI(TAG, "  FALL:  fall_status, fall_test, fall_reset");
    ESP_LOGI(TAG, "  System: reboot, free, tasks");
    ESP_LOGI(TAG, "========================================");
}
