// #include "cli.h"
// #include "fs.h"
// #include "ble.h"

// // #include "wifi_manager.h"
// // #include "wifi_cli.h"

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
//     // wifi_manager_init();
//     fs_init();
//     ble_init();
//     cli_init();

//     cli_register_fs();
//     cli_register_mem();
//     cli_register_i2c();
//     // wifi_cli_register();
//     cli_register_system();

//     xTaskCreate(cli_task, "cli", 4096, NULL, 5, NULL);
// }

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "cli.h"
#include "fs.h"
#include "mpu6050.h"
#include "fall_detection.h"
#include "config.h"

static const char *TAG = "MAIN";

// Fall event callback
static void on_fall_detected(fall_event_t *event)
{
    system_config_t *config = config_get();

    ESP_LOGW(TAG, "!!! FALL EVENT DETECTED !!!");
    ESP_LOGW(TAG, "Max Acceleration: %.2f g", event->max_accel);
    ESP_LOGW(TAG, "Orientation Change: %.2f degrees", event->orientation_change);

    // Log to file if enabled
    if (config->enable_log_to_file)
    {
        char log_entry[256];
        snprintf(log_entry, sizeof(log_entry),
                 "[%lu] FALL: MaxAccel=%.2f, OriChange=%.1f\n",
                 event->fall_timestamp, event->max_accel, event->orientation_change);

        FILE *f = fopen(config->log_file_path, "a");
        if (f)
        {
            fputs(log_entry, f);
            fclose(f);
            ESP_LOGI(TAG, "Fall event logged to %s", config->log_file_path);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to open log file");
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "System starting...");

    // Initialize filesystem first (required for config)
    fs_init();

    // Load configuration
    config_init();
    system_config_t *config = config_get();

    // Initialize I2C with config
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->i2c.sda_io_num,
        .scl_io_num = config->i2c.scl_io_num,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c.master.clk_speed,
    };

    // Sử dụng i2c_port từ config
    i2c_param_config(config->i2c_port, &i2c_cfg);
    i2c_driver_install(config->i2c_port, I2C_MODE_MASTER, 0, 0, 0);

    // Initialize MPU6050 với port từ config
    if (mpu6050_init(config->i2c_port) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MPU6050");
        return;
    }

    // Initialize fall detection with config
    fall_detection_init((fall_config_t *)&config->fall);
    fall_detection_register_callback(on_fall_detected);

    // Initialize CLI
    cli_init_all();

    // Auto-start fall detection if configured
    if (config->fall.auto_start)
    {
        ESP_LOGI(TAG, "Auto-starting fall detection...");
        // TODO: Add function to auto-start monitoring
    }

    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "Type 'help' for available commands");
    ESP_LOGI(TAG, "Commands: fall_start, fall_stop, fall_status, fall_config, mpu_read");
}