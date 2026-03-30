#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Forward declaration
typedef struct fall_config_s fall_config_t;
typedef struct mpu6050_data_s mpu6050_data_t;

// Three-step detection states
typedef enum
{
    STATE_NORMAL,
    STATE_FALL_DETECTED,  // Step 1: Impact detected
    STATE_INACTIVE_CHECK, // Step 2: Inactive period check
    STATE_FALL_CONFIRMED, // Step 3: Fall confirmed
} fall_state_t;

// Fall detection configuration
struct fall_config_s
{
    float impact_threshold;      // Impact threshold (g)
    float orientation_threshold; // Orientation change threshold (degrees)
    float inactivity_threshold;  // Inactivity period (seconds)
    float fall_timeout;          // Timeout for fall confirmation (seconds)
    int sample_rate_hz;          // Sample rate (Hz)
};

// Fall event data
typedef struct
{
    fall_state_t state;
    float max_accel;          // Maximum acceleration during impact
    float orientation_change; // Orientation change angle
    uint32_t fall_timestamp;  // Timestamp of fall detection
} fall_event_t;

// Callback for fall events
typedef void (*fall_callback_t)(fall_event_t *event);

// Function prototypes
esp_err_t fall_detection_init(fall_config_t *config);
esp_err_t fall_detection_process(mpu6050_data_t *sensor_data); // Sửa prototype
void fall_detection_register_callback(fall_callback_t callback);
fall_state_t fall_detection_get_state(void);
void fall_detection_reset(void);