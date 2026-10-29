#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUTTON_PIN 9

esp_err_t button_init(void);
bool button_is_pressed(void);
bool button_is_long_pressed(uint32_t hold_time_ms);

/* Trả về true nếu có sự kiện nhấn mới (đã chống dội), tự xóa cờ */
bool button_take_press_event(void);

/* Task sẽ được đánh thức ngay khi có cạnh xuống (nhấn nút) */
void button_set_notify_task(TaskHandle_t task);
