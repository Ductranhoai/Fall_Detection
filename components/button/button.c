#include "button.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "BUTTON";

static uint32_t s_press_start_time = 0;
static bool s_was_pressed = false;

static volatile bool s_press_event = false;
static volatile int64_t s_last_isr_us = 0;
static TaskHandle_t s_notify_task = NULL;

static void IRAM_ATTR button_isr(void *arg)
{
    (void)arg;

    int64_t now = esp_timer_get_time();
    if (now - s_last_isr_us < 30000) {
        return;
    }
    s_last_isr_us = now;
    s_press_event = true;

    BaseType_t woken = pdFALSE;
    if (s_notify_task) {
        vTaskNotifyGiveFromISR(s_notify_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

esp_err_t button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BUTTON_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "isr service failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(BUTTON_PIN, button_isr, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "isr handler add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    printf("[BUTTON] Init on GPIO%d (interrupt)\n", BUTTON_PIN);
    return ESP_OK;
}

bool button_is_pressed(void)
{
    return gpio_get_level(BUTTON_PIN) == 0;
}

bool button_take_press_event(void)
{
    if (!s_press_event) {
        return false;
    }
    s_press_event = false;
    return true;
}

void button_set_notify_task(TaskHandle_t task)
{
    s_notify_task = task;
}

bool button_is_long_pressed(uint32_t hold_time_ms)
{
    bool current = button_is_pressed();

    if (current && !s_was_pressed) {
        s_press_start_time = esp_timer_get_time() / 1000;
        s_was_pressed = true;
    }
    else if (!current && s_was_pressed) {
        uint32_t now = esp_timer_get_time() / 1000;
        uint32_t duration = now - s_press_start_time;
        s_was_pressed = false;
        return (duration >= hold_time_ms);
    }

    return false;
}
