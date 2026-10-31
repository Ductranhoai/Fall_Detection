#include "pwm_buzzer.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define BUZZER_TIMER    LEDC_TIMER_0
#define BUZZER_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_MODE     LEDC_LOW_SPEED_MODE
#define BUZZER_DUTY     128

static int s_buzzer_pin = -1;
static bool s_buzzer_init = false;
static volatile bool s_muted = false;

esp_err_t pwm_buzzer_init(int gpio_pin)
{
    if (s_buzzer_init) {
        return ESP_ERR_INVALID_STATE;
    }
    
    s_buzzer_pin = gpio_pin;
    
    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = BUZZER_TIMER,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);
    
    ledc_channel_config_t channel_conf = {
        .gpio_num = s_buzzer_pin,
        .speed_mode = BUZZER_MODE,
        .channel = BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);
    
    s_buzzer_init = true;
    printf("[PWM] Buzzer init on GPIO%d\n", gpio_pin);
    
    return ESP_OK;
}

void pwm_buzzer_play(uint32_t freq_hz, uint32_t duration_ms)
{
    if (!s_buzzer_init || s_muted) return;
    
    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = BUZZER_TIMER,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    if (s_muted) return;
    
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, BUZZER_DUTY);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);

    if (s_muted) {
        pwm_buzzer_stop();
        return;
    }
    
    if (duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        pwm_buzzer_stop();
    }
}

void pwm_buzzer_stop(void)
{
    if (!s_buzzer_init) return;
    
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

void pwm_buzzer_mute(bool mute)
{
    s_muted = mute;
    if (mute) {
        pwm_buzzer_stop();
    }
}

void pwm_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    pwm_buzzer_play(freq_hz, duration_ms);
}