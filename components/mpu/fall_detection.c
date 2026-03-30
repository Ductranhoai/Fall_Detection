#include "fall_detection.h"
#include "mpu6050.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "FALL_DETECTION";

// Static variables
static fall_config_t config;
static fall_state_t current_state = STATE_NORMAL;
static fall_event_t last_event = {0};
static fall_callback_t event_callback = NULL;
static uint32_t fall_start_time = 0;
static float orientation_angle = 0;
static bool impact_detected = false;

// Kalman filter for orientation estimation
typedef struct
{
    float Q_angle;   // Process noise covariance for angle
    float Q_bias;    // Process noise covariance for bias
    float R_measure; // Measurement noise covariance
    float angle;     // Estimated angle
    float bias;      // Estimated bias
    float P[2][2];   // Error covariance matrix
} kalman_filter_t;

static kalman_filter_t kalman_x, kalman_y, kalman_z;

static void kalman_init(kalman_filter_t *kf, float Q_angle, float Q_bias, float R_measure)
{
    kf->Q_angle = Q_angle;
    kf->Q_bias = Q_bias;
    kf->R_measure = R_measure;
    kf->angle = 0;
    kf->bias = 0;
    kf->P[0][0] = 0;
    kf->P[0][1] = 0;
    kf->P[1][0] = 0;
    kf->P[1][1] = 0;
}

static float calculate_accel_magnitude(mpu6050_data_t *data)
{
    return sqrt(data->accel_x * data->accel_x +
                data->accel_y * data->accel_y +
                data->accel_z * data->accel_z);
}

static float calculate_orientation(mpu6050_data_t *data)
{
    // Calculate roll and pitch from accelerometer
    float roll = atan2(data->accel_y, data->accel_z) * 180.0 / M_PI;
    float pitch = atan2(-data->accel_x, sqrt(data->accel_y * data->accel_y + data->accel_z * data->accel_z)) * 180.0 / M_PI;

    // Calculate orientation angle (absolute tilt from vertical)
    float angle = sqrt(roll * roll + pitch * pitch);

    return angle;
}

static float calculate_orientation_change(mpu6050_data_t *data, float last_angle)
{
    float current_angle = calculate_orientation(data);
    float change = fabs(current_angle - last_angle);

    // Handle wrap-around
    if (change > 180)
    {
        change = 360 - change;
    }

    return change;
}

static void check_impact(mpu6050_data_t *data)
{
    float accel_magnitude = calculate_accel_magnitude(data);

    if (accel_magnitude > config.impact_threshold && current_state == STATE_NORMAL)
    {
        // Impact detected
        current_state = STATE_FALL_DETECTED;
        fall_start_time = xTaskGetTickCount();
        last_event.max_accel = accel_magnitude;
        impact_detected = true;

        ESP_LOGI(TAG, "Impact detected! Magnitude: %.2f g", accel_magnitude);
    }
}

static void check_inactivity(mpu6050_data_t *data)
{
    if (current_state == STATE_FALL_DETECTED)
    {
        float accel_magnitude = calculate_accel_magnitude(data);

        // Check if user is inactive (low acceleration)
        if (accel_magnitude < (config.impact_threshold / 2))
        {
            uint32_t inactive_time = (xTaskGetTickCount() - fall_start_time) * portTICK_PERIOD_MS / 1000;

            if (inactive_time >= config.inactivity_threshold)
            {
                // Step 2: Inactive period confirmed
                current_state = STATE_INACTIVE_CHECK;
                ESP_LOGI(TAG, "Inactive period confirmed: %.1f seconds", (float)inactive_time);
            }
        }
    }
}

static void check_orientation_change(mpu6050_data_t *data)
{
    if (current_state == STATE_INACTIVE_CHECK)
    {
        float orientation_change = calculate_orientation_change(data, orientation_angle);

        if (orientation_change > config.orientation_threshold)
        {
            // Step 3: Fall confirmed
            current_state = STATE_FALL_CONFIRMED;
            last_event.orientation_change = orientation_change;
            last_event.state = STATE_FALL_CONFIRMED;
            last_event.fall_timestamp = xTaskGetTickCount();

            ESP_LOGW(TAG, "FALL CONFIRMED! Orientation change: %.1f degrees", orientation_change);

            // Trigger callback
            if (event_callback)
            {
                event_callback(&last_event);
            }

            // Log to file system
            char log_entry[256];
            snprintf(log_entry, sizeof(log_entry),
                     "FALL: Time=%lu, MaxAccel=%.2f, OriChange=%.1f\n",
                     last_event.fall_timestamp, last_event.max_accel, orientation_change);

            // Write to file (optional)
            FILE *f = fopen("/fatfs/fall_log.txt", "a");
            if (f)
            {
                fputs(log_entry, f);
                fclose(f);
            }
        }

        // Update orientation angle
        orientation_angle = calculate_orientation(data);
    }
}

static void reset_if_timeout(void)
{
    if (current_state != STATE_NORMAL)
    {
        uint32_t current_time = xTaskGetTickCount();
        uint32_t elapsed = (current_time - fall_start_time) * portTICK_PERIOD_MS / 1000;

        if (elapsed > config.fall_timeout)
        {
            current_state = STATE_NORMAL;
            impact_detected = false;
            ESP_LOGI(TAG, "Fall detection timeout, resetting to normal state");
        }
    }
}

esp_err_t fall_detection_init(fall_config_t *config_in)
{
    // Set default configuration if not provided
    if (config_in == NULL)
    {
        config.impact_threshold = 2.5f;
        config.orientation_threshold = 45.0f;
        config.inactivity_threshold = 2.0f;
        config.fall_timeout = 10.0f;
        config.sample_rate_hz = 50;
    }
    else
    {
        memcpy(&config, config_in, sizeof(fall_config_t));
    }

    // Initialize Kalman filters
    kalman_init(&kalman_x, 0.001f, 0.003f, 0.03f);
    kalman_init(&kalman_y, 0.001f, 0.003f, 0.03f);
    kalman_init(&kalman_z, 0.001f, 0.003f, 0.03f);

    current_state = STATE_NORMAL;
    impact_detected = false;

    ESP_LOGI(TAG, "Fall detection initialized");
    ESP_LOGI(TAG, "Config: Impact=%.1fg, Orientation=%.1fdeg, Inactivity=%.1fs",
             config.impact_threshold, config.orientation_threshold, config.inactivity_threshold);

    return ESP_OK;
}

esp_err_t fall_detection_process(mpu6050_data_t *sensor_data)
{
    static uint32_t last_time = 0;
    uint32_t current_time = xTaskGetTickCount();
    float dt = (current_time - last_time) * portTICK_PERIOD_MS / 1000.0;

    if (dt <= 0 || dt > 0.1)
    {
        dt = 0.02; // Default to 20ms if invalid
    }
    last_time = current_time;

    // Process based on current state
    switch (current_state)
    {
    case STATE_NORMAL:
        // Step 1: Detect impact
        check_impact(sensor_data);
        orientation_angle = calculate_orientation(sensor_data);
        break;

    case STATE_FALL_DETECTED:
        // Step 2: Check inactivity period
        check_inactivity(sensor_data);
        break;

    case STATE_INACTIVE_CHECK:
        // Step 3: Check orientation change
        check_orientation_change(sensor_data);
        break;

    case STATE_FALL_CONFIRMED:
        // Wait for reset
        break;
    }

    // Reset if timeout
    reset_if_timeout();

    return ESP_OK;
}

void fall_detection_register_callback(fall_callback_t callback)
{
    event_callback = callback;
}

fall_state_t fall_detection_get_state(void)
{
    return current_state;
}

void fall_detection_reset(void)
{
    current_state = STATE_NORMAL;
    impact_detected = false;
    ESP_LOGI(TAG, "Fall detection manually reset");
}