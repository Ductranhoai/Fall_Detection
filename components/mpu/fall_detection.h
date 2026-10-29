// fall_detection.h
#pragma once
#include "mpu6050.h"
#include <stdbool.h>

typedef enum
{
    FALL_STATE_NORMAL,
    FALL_STATE_FREE_FALL,
    FALL_STATE_IMPACT,
    FALL_STATE_TILT,
    FALL_STATE_FALL
} fall_state_t;

typedef struct
{
    float free_fall_threshold;
    float impact_threshold;
    float tilt_threshold;
    uint32_t free_fall_min_time;
    uint32_t impact_time_window;
    uint32_t tilt_time_window;
    uint32_t fall_confirm_time;
} fall_config_t;

typedef struct
{
    float accel_z_normal;
    float pitch_offset;
    float roll_offset;
    bool calibrated;
} fall_orientation_t;

typedef struct
{
    fall_state_t state;
    bool fall_detected;
    uint32_t fall_timestamp;
    float max_accel;
    float min_accel;
    float final_tilt;
    char detection_reason[64];
} fall_result_t;

esp_err_t fall_detection_init(const fall_config_t *config);
esp_err_t fall_detection_process(mpu6050_data_t *data);
fall_result_t fall_detection_get_result(void);
void fall_detection_reset(void);
void fall_detection_set_callback(void (*callback)(fall_result_t *result));
fall_config_t fall_get_default_config(void);
void fall_detection_calibrate_orientation(mpu6050_data_t *data);
void fall_detection_reset_orientation(void);
bool fall_detection_is_calibrated(void);
const fall_orientation_t *fall_detection_get_orientation(void);
const char *fall_state_to_string(fall_state_t state);
bool fall_detection_is_user_classified(void);
uint8_t fall_detection_get_user_type(void);
float fall_detection_get_dynamic_threshold(void);