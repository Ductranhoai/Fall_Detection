#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t pwm_buzzer_init(int gpio_pin);
void pwm_buzzer_play(uint32_t freq_hz, uint32_t duration_ms);
void pwm_buzzer_stop(void);
void pwm_buzzer_mute(bool mute);
void pwm_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms);