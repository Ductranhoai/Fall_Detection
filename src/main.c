#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "cli.h"
#include "fs.h"
// #include "wifi_manager.h"
// #include "wifi_cli.h"
#include "mpu_manager.h"
#include "mpu_config.h"
#include "fall_detection.h"

#include "bt_spp.h"

static const char *TAG = "MAIN";

static void on_mpu_data(mpu6050_data_t *data)
{
    fall_detection_process(data);
    fall_result_t result = fall_detection_get_result();

    if (result.fall_detected)
    {
        printf("\n>>> NGUOI DUNG BI TE! <<<\n\n");

        if (bt_spp_is_connected())
        {
            const char *msg = "FALL DETECTED!\n";
            bt_spp_send((const uint8_t *)msg, strlen(msg));
        }
    }
}

void app_main(void)
{
    printf("\n========================================\n");
    printf("     FALL DETECTION SYSTEM\n");
    printf("========================================\n\n");

    // wifi_manager_init();
    // vTaskDelay(pdMS_TO_TICKS(500));
    bt_spp_init();
    vTaskDelay(pdMS_TO_TICKS(300));

    fs_init();

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

    cli_init_all();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Available commands:");
    ESP_LOGI(TAG, "  WiFi: wifi_scan, wifi_status, wifi_connect, ifconfig");
    ESP_LOGI(TAG, "  MPU:  mpu_read, mpu_read -w, mpu_stop, mpu_cal, mpu_test");
    ESP_LOGI(TAG, "  FS:   ls, pwd, cd, cat, touch");
    ESP_LOGI(TAG, "  FALL: fall_status, fall_test, fall_reset");
    ESP_LOGI(TAG, "  System: reboot, free, tasks");

    ESP_LOGI(TAG, "========================================");
}