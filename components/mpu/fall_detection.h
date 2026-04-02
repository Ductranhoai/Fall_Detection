#pragma once
#include "mpu6050.h"
#include <stdbool.h>

// Fall detection states
typedef enum
{
    FALL_STATE_NORMAL,    // Normal state
    FALL_STATE_FREE_FALL, // Free fall detected
    FALL_STATE_IMPACT,    // Impact detected
    FALL_STATE_TILT,      // Tilt after fall
    FALL_STATE_FALL       // Fall confirmed
} fall_state_t;

// Fall detection configuration
typedef struct
{
    float free_fall_threshold;   // Free fall acceleration threshold (g)
    float impact_threshold;      // Impact acceleration threshold (g)
    float tilt_threshold;        // Tilt angle threshold (degrees)
    uint32_t free_fall_min_time; // Minimum free fall time (ms)
    uint32_t impact_time_window; // Time window after impact (ms)
    uint32_t tilt_time_window;   // Time window for tilt detection (ms)
    uint32_t fall_confirm_time;  // Time to confirm fall (ms)
} fall_config_t;

// Fall detection result
typedef struct
{
    fall_state_t state;
    bool fall_detected;
    uint32_t fall_timestamp;   // Timestamp when fall was detected
    float max_accel;           // Maximum acceleration during impact
    float min_accel;           // Minimum acceleration during free fall
    float final_tilt;          // Tilt angle after fall
    char detection_reason[64]; // Reason for detection
} fall_result_t;

// Function prototypes
esp_err_t fall_detection_init(const fall_config_t *config);
esp_err_t fall_detection_process(mpu6050_data_t *data);
fall_result_t fall_detection_get_result(void);
void fall_detection_reset(void);
void fall_detection_set_callback(void (*callback)(fall_result_t *result));
fall_config_t fall_get_default_config(void);

// Utility functions
const char *fall_state_to_string(fall_state_t state);