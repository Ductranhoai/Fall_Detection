#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "mpu_manager.h"
#include "mpu_config.h"
#include "fall_detection.h"

static const char *TAG = "MAIN";

static void on_mpu_data(mpu6050_data_t *data)
{
    fall_detection_process(data);

    fall_result_t result = fall_detection_get_result();

    if (result.fall_detected)
    {
        printf("\n========================================\n");
        printf("       NGUOI DUNG BI TE!\n");
        printf("========================================\n");

        printf("Reason      : %s\n", result.detection_reason);
        printf("Impact      : %.2fg\n", result.max_accel);
        printf("Final Tilt  : %.1f deg\n", result.final_tilt);

        printf("========================================\n\n");
    }
}

static void disable_jtag_pin(void)
{
    gpio_reset_pin(GPIO_NUM_4);

    gpio_set_direction(
        GPIO_NUM_4,
        GPIO_MODE_OUTPUT
    );

    ESP_LOGI(TAG, "GPIO reclaimed from JTAG");
}

void app_main(void)
{
    disable_jtag_pin();

    printf("\n========================================\n");
    printf("     FALL DETECTION SYSTEM\n");
    printf("========================================\n\n");

    vTaskDelay(pdMS_TO_TICKS(300));

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

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "========================================");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}