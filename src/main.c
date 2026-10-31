#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "mpu_manager.h"
#include "mpu_config.h"
#include "fall_detection.h"
#include "pwm_buzzer.h"
#include "button.h"
#include "wifi_manager.h"
#include "bt_manager.h"

static const char *TAG = "MAIN";
static volatile bool s_fall_reported = false;
static volatile bool s_alarm_active = false;
static volatile bool s_cooldown = false;
static TaskHandle_t s_alarm_task_handle = NULL;
static TaskHandle_t s_button_task_handle = NULL;

static void alarm_sos_4khz(void);
static void alarm_task(void *arg);
static void start_alarm_loop(void);
static void stop_alarm_loop(void);

static void start_alarm_loop(void)
{
    if (s_alarm_active) {
        printf("[DEBUG] Already active\n");
        return;
    }
    if (s_cooldown) {
        printf("[DEBUG] In cooldown\n");
        return;
    }
    
    s_alarm_active = true;
    pwm_buzzer_mute(false);
    printf("[ALARM] Triggered\n");
    if (s_alarm_task_handle) {
        xTaskNotifyGive(s_alarm_task_handle);
    }
}

static void stop_alarm_loop(void)
{
    if (!s_alarm_active) {
        printf("[DEBUG] Not active\n");
        return;
    }
    
    s_alarm_active = false;
    s_cooldown = true;
    pwm_buzzer_mute(true);
    printf("[ALARM] Stopped\n");
    bt_manager_notify_text("CLEAR");
    if (s_alarm_task_handle) {
        xTaskNotifyGive(s_alarm_task_handle);
    }
}

static void alarm_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!s_alarm_active || s_cooldown) {
            continue;
        }
        
        printf("[ALARM] Alarm loop started\n");
        
        while (s_alarm_active) {
            for (int i = 0; i < 30; i++) {
                if (!s_alarm_active) break;
                alarm_sos_4khz();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            if (!s_alarm_active) break;
            for (int i = 0; i < 30; i++) {
                if (!s_alarm_active) break;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        
        printf("[ALARM] Alarm loop stopped\n");
        pwm_buzzer_stop();
    }
}

static void button_task(void *arg)
{
    static uint32_t press_start_time = 0;
    static bool is_pressing = false;
    static uint32_t cooldown_start = 0;
    static bool prev_pressed = false;

    while (1) {
        /* Ngủ tối đa 50ms, nhưng thức ngay khi có ngắt nhấn nút */
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));

        bool pressed = button_is_pressed();
        bool fresh_press = button_take_press_event();

        if (s_cooldown) {
            if (cooldown_start == 0) {
                cooldown_start = esp_timer_get_time() / 1000;
            }
            if ((esp_timer_get_time() / 1000) - cooldown_start >= 2000) {
                s_cooldown = false;
                cooldown_start = 0;
            }
        }

        /* Bấm là tắt còi ngay lập tức khi đang kêu */
        if (s_alarm_active && (fresh_press || (pressed && !prev_pressed))) {
            printf("[BUTTON] Stop button pressed\n");
            stop_alarm_loop();
            is_pressing = false;
            prev_pressed = pressed;
            continue;
        }

        if (!s_alarm_active && !s_cooldown) {
            if (pressed && !is_pressing) {
                press_start_time = esp_timer_get_time() / 1000;
                is_pressing = true;
            }

            if (pressed && is_pressing) {
                int64_t duration = (esp_timer_get_time() / 1000) - press_start_time;
                if (duration >= 5000) {
                    start_alarm_loop();
                    is_pressing = false;
                }
            }

            if (!pressed) {
                is_pressing = false;
            }
        } else {
            is_pressing = false;
        }

        prev_pressed = pressed;
    }
}

static void on_mpu_data(mpu6050_data_t *data)
{
    fall_detection_process(data);
    fall_result_t result = fall_detection_get_result();

    if (result.fall_detected)
    {
        if (!s_fall_reported)
        {
            s_fall_reported = true;

            if (!s_cooldown)
            {
                printf("[FALL] Fall detected! Starting alarm...\n");
                start_alarm_loop();

                printf("\n========================================\n");
                printf("       NGUOI DUNG BI TE!\n");
                printf("========================================\n");
                printf("Reason      : %s\n", result.detection_reason);
                printf("Impact      : %.2fg\n", result.max_accel);
                printf("Final Tilt  : %.1f deg\n", result.final_tilt);
                printf("========================================\n\n");

                char ble_msg[24];
                bt_manager_notify_text("FALL");
                snprintf(ble_msg, sizeof(ble_msg), "I:%.2fg", result.max_accel);
                bt_manager_notify_text(ble_msg);
                snprintf(ble_msg, sizeof(ble_msg), "T:%.1f", result.final_tilt);
                bt_manager_notify_text(ble_msg);
            }
        }
    }
    else
    {
        s_fall_reported = false;
    }
}

static void alarm_sos_4khz(void)
{
    for (int i = 0; i < 3; i++) {
        pwm_buzzer_beep(4000, 150);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    for (int i = 0; i < 3; i++) {
        pwm_buzzer_beep(4000, 450);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(300));
    
    for (int i = 0; i < 3; i++) {
        pwm_buzzer_beep(4000, 150);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void net_status_task(void *arg)
{
    bool was_up = false;
    bool was_connecting = false;

    while (1)
    {
        bool up = wifi_manager_is_connected();

        if (up != was_up)
        {
            if (up)
            {
                bt_manager_notify_text("WIFI:UP");
                const char *ip = wifi_manager_get_ip();
                if (ip && ip[0])
                {
                    char buf[24];
                    snprintf(buf, sizeof(buf), "IP:%s", ip);
                    bt_manager_notify_text(buf);
                }
            }
            else
            {
                bt_manager_notify_text("WIFI:DOWN");
            }
            was_up = up;
        }

        bool connecting = wifi_manager_is_connecting();
        if (was_connecting && !connecting && !up)
        {
            bt_manager_notify_text("WIFI:FAIL");
        }
        was_connecting = connecting;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    printf("     FALL DETECTION SYSTEM\n");
    vTaskDelay(pdMS_TO_TICKS(300));

    pwm_buzzer_init(0);
    button_init();

    pwm_buzzer_beep(4000, 200);
    vTaskDelay(pdMS_TO_TICKS(100));
    pwm_buzzer_beep(4000, 200);

    xTaskCreate(alarm_task, "alarm_task", 3072, NULL, 5, &s_alarm_task_handle);
    xTaskCreate(button_task, "button_task", 3072, NULL, 5, &s_button_task_handle);
    button_set_notify_task(s_button_task_handle);

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

    wifi_manager_init();
    printf("[OK] WiFi station started\n");

    if (bt_manager_init() == ESP_OK)
    {
        printf("[OK] BLE ready, name=FallDetector\n");
    }
    else
    {
        printf("[ERROR] BLE init failed!\n");
    }

    xTaskCreate(net_status_task, "net_status", 3072, NULL, 4, NULL);

    ESP_LOGI(TAG, "System ready!");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
