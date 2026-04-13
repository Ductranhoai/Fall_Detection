#include "fall_detection.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Static variables
static fall_config_t s_config;
static fall_result_t s_result;
static bool s_initialized = false;
static void (*s_callback)(fall_result_t *result) = NULL;

// State machine variables
static fall_state_t s_current_state = FALL_STATE_NORMAL;
static uint32_t s_free_fall_start_time = 0;
static uint32_t s_impact_time = 0;
static float s_max_accel = 0;
static float s_min_accel = 3.0;
static float s_accel_z_before_fall = 1.0;

// Orientation calibration
static fall_orientation_t s_orientation = {1.0f, 0.0f, 0.0f, false};

// Get default configuration
fall_config_t fall_get_default_config(void)
{
    fall_config_t config = {
        .free_fall_threshold = 0.5,
        .impact_threshold = 2.5,
        .tilt_threshold = 45.0,
        .free_fall_min_time = 100,
        .impact_time_window = 200,
        .tilt_time_window = 2000,
        .fall_confirm_time = 500};
    return config;
}

// Initialize fall detection
esp_err_t fall_detection_init(const fall_config_t *config)
{
    if (s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(&s_config, config, sizeof(fall_config_t));
    fall_detection_reset();

    s_initialized = true;
    printf("[FALL] Init OK - Thresholds: free=%.2fg impact=%.2fg tilt=%.1f\n",
           s_config.free_fall_threshold, s_config.impact_threshold, s_config.tilt_threshold);
    printf("[FALL] Auto-calibration will run in 3 seconds...\n");

    return ESP_OK;
}

// Reset detection state
void fall_detection_reset(void)
{
    s_current_state = FALL_STATE_NORMAL;
    s_result.state = FALL_STATE_NORMAL;
    s_result.fall_detected = false;
    s_result.fall_timestamp = 0;
    s_result.max_accel = 0;
    s_result.min_accel = 3.0;
    s_result.final_tilt = 0;
    strcpy(s_result.detection_reason, "Normal");

    s_free_fall_start_time = 0;
    s_impact_time = 0;
    s_max_accel = 0;
    s_min_accel = 3.0;
}

// Set callback function
void fall_detection_set_callback(void (*callback)(fall_result_t *result))
{
    s_callback = callback;
}

// Orientation calibration functions
void fall_detection_calibrate_orientation(mpu6050_data_t *data)
{
    if (data)
    {
        s_orientation.accel_z_normal = data->accel_z;
        s_orientation.pitch_offset = data->pitch;
        s_orientation.roll_offset = data->roll;
    }
    s_orientation.calibrated = true;

    printf("[FALL] Calibrated: Z=%.2fg, Pitch=%.1f, Roll=%.1f\n",
           s_orientation.accel_z_normal, s_orientation.pitch_offset, s_orientation.roll_offset);
}

void fall_detection_reset_orientation(void)
{
    s_orientation.calibrated = false;
    s_orientation.accel_z_normal = 1.0f;
    s_orientation.pitch_offset = 0.0f;
    s_orientation.roll_offset = 0.0f;
    printf("[FALL] Orientation reset\n");
}

bool fall_detection_is_calibrated(void)
{
    return s_orientation.calibrated;
}

const fall_orientation_t *fall_detection_get_orientation(void)
{
    return &s_orientation;
}

// Process MPU data for fall detection
esp_err_t fall_detection_process(mpu6050_data_t *data)
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t current_time = esp_timer_get_time() / 1000;
    float total_accel = sqrt(data->accel_x * data->accel_x +
                             data->accel_y * data->accel_y +
                             data->accel_z * data->accel_z);

    // Auto-calibration
    static int stable_count = 0;
    static int drift_count = 0;
    static float prev_z = 0, prev_pitch = 0, prev_roll = 0;

    if (s_current_state == FALL_STATE_NORMAL)
    {
        if (!s_orientation.calibrated)
        {
            float delta_z = fabs(data->accel_z - prev_z);
            float delta_pitch = fabs(data->pitch - prev_pitch);
            float delta_roll = fabs(data->roll - prev_roll);

            if (delta_z < 0.05f && delta_pitch < 1.0f && delta_roll < 1.0f)
            {
                stable_count++;
                if (stable_count >= 30)
                {
                    fall_detection_calibrate_orientation(data);
                    stable_count = 0;
                }
            }
            else
            {
                stable_count = 0;
            }
        }
        else
        {
            float z_diff = fabs(data->accel_z - s_orientation.accel_z_normal);
            float pitch_diff = fabs(data->pitch - s_orientation.pitch_offset);
            float roll_diff = fabs(data->roll - s_orientation.roll_offset);

            if (z_diff > 0.3f || pitch_diff > 10.0f || roll_diff > 10.0f)
            {
                drift_count++;
                if (drift_count >= 20)
                {
                    printf("[FALL] Position changed! Z:%.2f->%.2f (diff:%.2f), Pitch:%.1f->%.1f (diff:%.1f)\n",
                           s_orientation.accel_z_normal, data->accel_z, z_diff,
                           s_orientation.pitch_offset, data->pitch, pitch_diff);

                    s_orientation.calibrated = false;
                    drift_count = 0;
                }
            }
            else
            {
                drift_count = 0;
            }
        }

        prev_z = data->accel_z;
        prev_pitch = data->pitch;
        prev_roll = data->roll;
    }

    // State machine
    switch (s_current_state)
    {
    case FALL_STATE_NORMAL:
        if (total_accel < s_config.free_fall_threshold)
        {
            s_current_state = FALL_STATE_FREE_FALL;
            s_free_fall_start_time = current_time;
            s_min_accel = total_accel;
            printf("[FALL] Free fall: %.2fg\n", total_accel);
        }
        break;

    case FALL_STATE_FREE_FALL:
        if (total_accel < s_min_accel)
        {
            s_min_accel = total_accel;
        }

        if (current_time - s_free_fall_start_time >= s_config.free_fall_min_time)
        {
            s_current_state = FALL_STATE_IMPACT;
            s_impact_time = current_time;
            printf("[FALL] Impact waiting...\n");
        }

        if (total_accel > 0.8)
        {
            s_current_state = FALL_STATE_NORMAL;
        }
        break;

    case FALL_STATE_IMPACT:
        if (total_accel > s_config.impact_threshold)
        {
            if (total_accel > s_max_accel)
            {
                s_max_accel = total_accel;
            }

            if (current_time - s_impact_time <= s_config.impact_time_window)
            {
                s_current_state = FALL_STATE_TILT;
                printf("[FALL] Impact: %.2fg\n", s_max_accel);
            }
            else
            {
                s_current_state = FALL_STATE_NORMAL;
            }
        }

        if (current_time - s_impact_time > s_config.impact_time_window)
        {
            s_current_state = FALL_STATE_NORMAL;
        }
        break;

    case FALL_STATE_TILT:
    {
        float pitch_diff = fabs(data->pitch - s_orientation.pitch_offset);
        float roll_diff = fabs(data->roll - s_orientation.roll_offset);
        float tilt_angle = pitch_diff > roll_diff ? pitch_diff : roll_diff;

        if (!s_orientation.calibrated)
        {
            tilt_angle = fabs(data->pitch) > fabs(data->roll) ? fabs(data->pitch) : fabs(data->roll);
        }

        if (tilt_angle > s_config.tilt_threshold)
        {
            if (current_time - s_impact_time >= s_config.fall_confirm_time)
            {
                s_current_state = FALL_STATE_FALL;
                s_result.fall_detected = true;
                s_result.fall_timestamp = current_time;
                s_result.max_accel = s_max_accel;
                s_result.min_accel = s_min_accel;
                s_result.final_tilt = tilt_angle;
                s_result.state = FALL_STATE_FALL;

                snprintf(s_result.detection_reason, sizeof(s_result.detection_reason),
                         "Free fall: %.2fg, Impact: %.2fg, Tilt: %.1f",
                         s_min_accel, s_max_accel, s_result.final_tilt);

                printf("\n========================================\n");
                printf("  NGUOI DUNG BI TE!\n");
                printf("  Free fall: %.2fg | Impact: %.2fg | Tilt: %.1f\n",
                       s_min_accel, s_max_accel, s_result.final_tilt);
                printf("========================================\n\n");

                if (s_callback)
                {
                    s_callback(&s_result);
                }
            }
        }
        else
        {
            if (current_time - s_impact_time > s_config.tilt_time_window)
            {
                s_current_state = FALL_STATE_NORMAL;
                fall_detection_reset();
            }
        }
        break;
    }

    case FALL_STATE_FALL:
        if (current_time - s_result.fall_timestamp > 10000)
        {
            fall_detection_reset();
        }
        break;
    }

    s_result.state = s_current_state;
    return ESP_OK;
}

// Get current fall detection result
fall_result_t fall_detection_get_result(void)
{
    return s_result;
}

// Convert state to string
const char *fall_state_to_string(fall_state_t state)
{
    switch (state)
    {
    case FALL_STATE_NORMAL:
        return "NORMAL";
    case FALL_STATE_FREE_FALL:
        return "FREE_FALL";
    case FALL_STATE_IMPACT:
        return "IMPACT";
    case FALL_STATE_TILT:
        return "TILT";
    case FALL_STATE_FALL:
        return "FALL";
    default:
        return "UNKNOWN";
    }
}