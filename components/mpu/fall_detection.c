#include "fall_detection.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define FALL_COOLDOWN_MS      10000
#define GRAVITY_LP_ALPHA      0.08f
#define GRAVITY_MIN_MAG       0.75f
#define GRAVITY_MAX_MAG       1.25f
#define STILL_MIN_MAG         0.60f
#define STILL_MAX_MAG         1.40f
#define MIN_VALID_DIR_MAG     0.30f
#define RAD_TO_DEG            57.2957795f

static fall_config_t s_config;
static fall_result_t s_result;
static bool s_initialized = false;
static void (*s_callback)(fall_result_t *result) = NULL;

static fall_state_t s_state = FALL_STATE_NORMAL;
static uint32_t s_free_fall_start = 0;
static uint32_t s_low_accel_start = 0;
static bool s_low_accel_active = false;
static uint32_t s_impact_time = 0;
static uint32_t s_lying_start = 0;
static float s_max_accel = 0.0f;
static float s_min_accel = 3.0f;

static float s_gravity_ref[3] = {0.0f, 0.0f, 1.0f};
static bool s_gravity_ref_valid = false;
static float s_pre_fall_dir[3] = {0.0f, 0.0f, 1.0f};

static fall_orientation_t s_orientation = {1.0f, 0.0f, 0.0f, false};

static void reset_cycle(void)
{
    s_state = FALL_STATE_NORMAL;
    s_free_fall_start = 0;
    s_low_accel_start = 0;
    s_low_accel_active = false;
    s_impact_time = 0;
    s_lying_start = 0;
    s_max_accel = 0.0f;
    s_min_accel = 3.0f;

    s_result.state = FALL_STATE_NORMAL;
    s_result.fall_detected = false;
    s_result.fall_timestamp = 0;
    s_result.max_accel = 0.0f;
    s_result.min_accel = 3.0f;
    s_result.final_tilt = 0.0f;
    strcpy(s_result.detection_reason, "Normal");
}

static float vector_magnitude(const mpu6050_data_t *data)
{
    return sqrtf(data->accel_x * data->accel_x +
                 data->accel_y * data->accel_y +
                 data->accel_z * data->accel_z);
}

static bool normalize_accel(const mpu6050_data_t *data, float mag, float out[3])
{
    if (mag < MIN_VALID_DIR_MAG) {
        return false;
    }
    out[0] = data->accel_x / mag;
    out[1] = data->accel_y / mag;
    out[2] = data->accel_z / mag;
    return true;
}

static void update_gravity_reference(const mpu6050_data_t *data, float mag)
{
    if (mag < GRAVITY_MIN_MAG || mag > GRAVITY_MAX_MAG) {
        return;
    }

    float unit[3] = {
        data->accel_x / mag,
        data->accel_y / mag,
        data->accel_z / mag};

    if (!s_gravity_ref_valid) {
        memcpy(s_gravity_ref, unit, sizeof(s_gravity_ref));
        s_gravity_ref_valid = true;
        return;
    }

    for (int i = 0; i < 3; i++) {
        s_gravity_ref[i] += GRAVITY_LP_ALPHA * (unit[i] - s_gravity_ref[i]);
    }

    float norm = sqrtf(s_gravity_ref[0] * s_gravity_ref[0] +
                       s_gravity_ref[1] * s_gravity_ref[1] +
                       s_gravity_ref[2] * s_gravity_ref[2]);
    if (norm > 0.0001f) {
        for (int i = 0; i < 3; i++) {
            s_gravity_ref[i] /= norm;
        }
    }
}

static float angle_between(const float a[3], const float b[3])
{
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;
    return acosf(dot) * RAD_TO_DEG;
}

static void remember_pre_fall_orientation(void)
{
    if (s_gravity_ref_valid) {
        memcpy(s_pre_fall_dir, s_gravity_ref, sizeof(s_pre_fall_dir));
    } else {
        s_pre_fall_dir[0] = 0.0f;
        s_pre_fall_dir[1] = 0.0f;
        s_pre_fall_dir[2] = 1.0f;
    }
}

static void confirm_fall(uint32_t now, float tilt)
{
    s_state = FALL_STATE_FALL;

    s_result.state = FALL_STATE_FALL;
    s_result.fall_detected = true;
    s_result.fall_timestamp = now;
    s_result.max_accel = s_max_accel;
    s_result.min_accel = s_min_accel;
    s_result.final_tilt = tilt;

    snprintf(s_result.detection_reason, sizeof(s_result.detection_reason),
             "Free:%.2fg Impact:%.2fg Tilt:%.1f",
             s_min_accel, s_max_accel, tilt);

    printf("[FALL] DETECTED! Impact:%.2fg Tilt:%.1f deg\n", s_max_accel, tilt);

    if (s_callback) {
        s_callback(&s_result);
    }
}

fall_config_t fall_get_default_config(void)
{
    fall_config_t config = {
        .free_fall_threshold = 0.60f,
        .impact_threshold = 2.50f,
        .tilt_threshold = 45.0f,
        .free_fall_min_time = 100,
        .impact_time_window = 800,
        .tilt_time_window = 2500,
        .fall_confirm_time = 800};
    return config;
}

esp_err_t fall_detection_init(const fall_config_t *config)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_config, config, sizeof(fall_config_t));
    reset_cycle();

    s_gravity_ref[0] = 0.0f;
    s_gravity_ref[1] = 0.0f;
    s_gravity_ref[2] = 1.0f;
    s_gravity_ref_valid = false;

    s_orientation.accel_z_normal = 1.0f;
    s_orientation.pitch_offset = 0.0f;
    s_orientation.roll_offset = 0.0f;
    s_orientation.calibrated = false;

    s_initialized = true;
    printf("[FALL] Init OK\n");

    return ESP_OK;
}

void fall_detection_reset(void)
{
    reset_cycle();
}

void fall_detection_set_callback(void (*callback)(fall_result_t *result))
{
    s_callback = callback;
}

void fall_detection_calibrate_orientation(mpu6050_data_t *data)
{
    if (!data) {
        return;
    }

    float mag = vector_magnitude(data);
    float dir[3];
    if (!normalize_accel(data, mag, dir)) {
        printf("[FALL] Calibration ignored (invalid data)\n");
        return;
    }

    memcpy(s_gravity_ref, dir, sizeof(s_gravity_ref));
    s_gravity_ref_valid = true;

    s_orientation.accel_z_normal = data->accel_z;
    s_orientation.pitch_offset = data->pitch;
    s_orientation.roll_offset = data->roll;
    s_orientation.calibrated = true;
    printf("[FALL] Calibrated: Z=%.2fg\n", s_orientation.accel_z_normal);
}

void fall_detection_reset_orientation(void)
{
    s_orientation.calibrated = false;
    s_orientation.accel_z_normal = 1.0f;
    s_orientation.pitch_offset = 0.0f;
    s_orientation.roll_offset = 0.0f;
    s_gravity_ref_valid = false;
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

bool fall_detection_is_user_classified(void)
{
    return false;
}

uint8_t fall_detection_get_user_type(void)
{
    return 1;
}

float fall_detection_get_dynamic_threshold(void)
{
    return s_config.impact_threshold;
}

esp_err_t fall_detection_process(mpu6050_data_t *data)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    float mag = vector_magnitude(data);

    switch (s_state)
    {
    case FALL_STATE_NORMAL:
        update_gravity_reference(data, mag);

        if (mag < s_config.free_fall_threshold) {
            if (!s_low_accel_active) {
                s_low_accel_start = now;
                s_low_accel_active = true;
            } else if (now - s_low_accel_start >= s_config.free_fall_min_time) {
                s_state = FALL_STATE_FREE_FALL;
                s_free_fall_start = s_low_accel_start;
                s_min_accel = mag;
                s_low_accel_active = false;
                remember_pre_fall_orientation();
                printf("[FALL] Free fall: %.2fg\n", mag);
            }
        } else {
            s_low_accel_active = false;
        }
        break;

    case FALL_STATE_FREE_FALL:
        if (mag < s_min_accel) {
            s_min_accel = mag;
        }

        if (mag > s_config.impact_threshold) {
            s_state = FALL_STATE_IMPACT;
            s_impact_time = now;
            s_lying_start = 0;
            s_max_accel = mag;
            printf("[FALL] Impact: %.2fg\n", mag);
        } else if (now - s_free_fall_start >
                   (s_config.free_fall_min_time + s_config.impact_time_window)) {
            reset_cycle();
            printf("[FALL] No impact, aborted\n");
        }
        break;

    case FALL_STATE_IMPACT:
    {
        if (mag > s_max_accel) {
            s_max_accel = mag;
        }

        if (now - s_impact_time > s_config.tilt_time_window) {
            reset_cycle();
            break;
        }

        if (now - s_impact_time < s_config.impact_time_window && mag > s_config.impact_threshold) {
            break;
        }

        float dir[3];
        if (normalize_accel(data, mag, dir)) {
            float tilt = angle_between(dir, s_pre_fall_dir);
            if (tilt >= s_config.tilt_threshold) {
                s_state = FALL_STATE_TILT;
                s_lying_start = now;
                printf("[FALL] Tilt: %.1f deg\n", tilt);
            } else if (now - s_impact_time > s_config.impact_time_window) {
                reset_cycle();
                printf("[FALL] No tilt, not a fall\n");
            }
        }
        break;
    }

    case FALL_STATE_TILT:
    {
        if (now - s_impact_time > s_config.tilt_time_window) {
            reset_cycle();
            printf("[FALL] Confirm timeout\n");
            break;
        }

        float dir[3];
        if (normalize_accel(data, mag, dir)) {
            float tilt = angle_between(dir, s_pre_fall_dir);
            bool lying = tilt >= s_config.tilt_threshold;
            bool still = mag >= STILL_MIN_MAG && mag <= STILL_MAX_MAG;

            if (lying && still) {
                if (s_lying_start == 0) {
                    s_lying_start = now;
                } else if (now - s_lying_start >= s_config.fall_confirm_time) {
                    confirm_fall(now, tilt);
                    break;
                }
            } else {
                s_lying_start = 0;
            }
        } else {
            s_lying_start = 0;
        }
        break;
    }

    case FALL_STATE_FALL:
        if (now - s_result.fall_timestamp > FALL_COOLDOWN_MS) {
            reset_cycle();
        }
        break;
    }

    if (s_state != FALL_STATE_FALL) {
        s_result.state = s_state;
    }

    return ESP_OK;
}

fall_result_t fall_detection_get_result(void)
{
    return s_result;
}

const char *fall_state_to_string(fall_state_t state)
{
    switch (state) {
    case FALL_STATE_NORMAL: return "NORMAL";
    case FALL_STATE_FREE_FALL: return "FREE_FALL";
    case FALL_STATE_IMPACT: return "IMPACT";
    case FALL_STATE_TILT: return "TILT";
    case FALL_STATE_FALL: return "FALL";
    default: return "UNKNOWN";
    }
}
