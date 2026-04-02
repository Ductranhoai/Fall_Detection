#include "fall_detection.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>

static const char *TAG = "FALL_DETECT";

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
static float s_min_accel = 3.0;  // Start with high value
static float s_accel_z_before_fall = 1.0;

// Statistics for fall detection
typedef struct {
    float accel_buffer[10];
    int buffer_index;
    float accel_z_smooth;
} filter_data_t;

static filter_data_t s_filter;

// Simple moving average filter
static float moving_average_filter(float new_value)
{
    static int count = 0;
    s_filter.accel_buffer[s_filter.buffer_index] = new_value;
    s_filter.buffer_index = (s_filter.buffer_index + 1) % 10;
    
    if (count < 10) count++;
    
    float sum = 0;
    for (int i = 0; i < count; i++) {
        sum += s_filter.accel_buffer[i];
    }
    
    s_filter.accel_z_smooth = sum / count;
    return s_filter.accel_z_smooth;
}

// Get default configuration
fall_config_t fall_get_default_config(void)
{
    fall_config_t config = {
        .free_fall_threshold = 0.5,      // Below 0.5g = free fall
        .impact_threshold = 2.5,         // Above 2.5g = impact
        .tilt_threshold = 45.0,          // Tilt > 45 degrees
        .free_fall_min_time = 100,        // Free fall for at least 100ms
        .impact_time_window = 200,        // Check impact within 200ms after free fall
        .tilt_time_window = 2000,         // Check tilt within 2 seconds after impact
        .fall_confirm_time = 500          // Confirm fall after 500ms of tilt
    };
    return config;
}

// Initialize fall detection
esp_err_t fall_detection_init(const fall_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Fall detection already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Copy configuration
    memcpy(&s_config, config, sizeof(fall_config_t));
    
    // Initialize filter buffer
    memset(&s_filter, 0, sizeof(filter_data_t));
    s_filter.buffer_index = 0;
    
    // Reset result
    fall_detection_reset();
    
    s_initialized = true;
    ESP_LOGI(TAG, "Fall detection initialized");
    ESP_LOGI(TAG, "  Free fall threshold: %.2fg", s_config.free_fall_threshold);
    ESP_LOGI(TAG, "  Impact threshold: %.2fg", s_config.impact_threshold);
    ESP_LOGI(TAG, "  Tilt threshold: %.1f°", s_config.tilt_threshold);
    
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

// Process MPU data for fall detection
esp_err_t fall_detection_process(mpu6050_data_t *data)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;  // Convert to ms
    
    // Apply filter to reduce noise
    float accel_z_filtered = moving_average_filter(data->accel_z);
    float total_accel = sqrt(data->accel_x * data->accel_x + 
                              data->accel_y * data->accel_y + 
                              data->accel_z * data->accel_z);
    
    // State machine for fall detection
    switch (s_current_state) {
        case FALL_STATE_NORMAL:
            // Check for free fall (total acceleration near 0)
            if (total_accel < s_config.free_fall_threshold) {
                s_current_state = FALL_STATE_FREE_FALL;
                s_free_fall_start_time = current_time;
                s_min_accel = total_accel;
                s_accel_z_before_fall = data->accel_z;
                ESP_LOGD(TAG, "Free fall detected! Accel: %.2fg", total_accel);
            }
            break;
            
        case FALL_STATE_FREE_FALL:
            // Update minimum acceleration
            if (total_accel < s_min_accel) {
                s_min_accel = total_accel;
            }
            
            // Check if free fall duration is sufficient
            if (current_time - s_free_fall_start_time >= s_config.free_fall_min_time) {
                // Free fall confirmed, now look for impact
                s_current_state = FALL_STATE_IMPACT;
                s_impact_time = current_time;
                ESP_LOGD(TAG, "Free fall confirmed for %d ms", 
                         current_time - s_free_fall_start_time);
            }
            
            // If acceleration returns to normal before sufficient time, reset
            if (total_accel > 0.8) {
                ESP_LOGD(TAG, "Free fall ended early, resetting");
                s_current_state = FALL_STATE_NORMAL;
            }
            break;
            
        case FALL_STATE_IMPACT:
            // Check for impact (high acceleration)
            if (total_accel > s_config.impact_threshold) {
                if (total_accel > s_max_accel) {
                    s_max_accel = total_accel;
                }
                
                // Check if impact occurs within time window
                if (current_time - s_impact_time <= s_config.impact_time_window) {
                    s_current_state = FALL_STATE_TILT;
                    ESP_LOGD(TAG, "Impact detected! Max: %.2fg", s_max_accel);
                } else {
                    // Impact too late, reset
                    ESP_LOGD(TAG, "Impact too late, resetting");
                    s_current_state = FALL_STATE_NORMAL;
                }
            }
            
            // Timeout if no impact detected
            if (current_time - s_impact_time > s_config.impact_time_window) {
                ESP_LOGD(TAG, "No impact detected, resetting");
                s_current_state = FALL_STATE_NORMAL;
            }
            break;
            
        case FALL_STATE_TILT:
            // Check for abnormal tilt after fall
            if (fabs(data->pitch) > s_config.tilt_threshold || 
                fabs(data->roll) > s_config.tilt_threshold) {
                
                // Check if tilt persists
                if (current_time - s_impact_time >= s_config.fall_confirm_time) {
                    // Fall confirmed!
                    s_current_state = FALL_STATE_FALL;
                    s_result.fall_detected = true;
                    s_result.fall_timestamp = current_time;
                    s_result.max_accel = s_max_accel;
                    s_result.min_accel = s_min_accel;
                    s_result.final_tilt = fabs(data->pitch) > fabs(data->roll) ? 
                                          fabs(data->pitch) : fabs(data->roll);
                    s_result.state = FALL_STATE_FALL;
                    
                    // Create reason string
                    snprintf(s_result.detection_reason, sizeof(s_result.detection_reason),
                             "Free fall: %.2fg, Impact: %.2fg, Tilt: %.1f°",
                             s_min_accel, s_max_accel, s_result.final_tilt);
                    
                    ESP_LOGW(TAG, "=========================================");
                    ESP_LOGW(TAG, "⚠️  FALL DETECTED! ⚠️");
                    ESP_LOGW(TAG, "  Free fall: %.2fg", s_min_accel);
                    ESP_LOGW(TAG, "  Impact: %.2fg", s_max_accel);
                    ESP_LOGW(TAG, "  Final tilt: %.1f°", s_result.final_tilt);
                    ESP_LOGW(TAG, "  Pitch: %.1f°, Roll: %.1f°", data->pitch, data->roll);
                    ESP_LOGW(TAG, "=========================================");
                    
                    // Call callback if registered
                    if (s_callback) {
                        s_callback(&s_result);
                    }
                }
            } else {
                // Device returned to normal orientation, maybe not a fall
                if (current_time - s_impact_time > s_config.tilt_time_window) {
                    ESP_LOGD(TAG, "No sustained tilt, resetting");
                    s_current_state = FALL_STATE_NORMAL;
                    fall_detection_reset();
                }
            }
            break;
            
        case FALL_STATE_FALL:
            // After fall is detected, we can implement cooldown period
            // Reset after a certain time or manually
            if (current_time - s_result.fall_timestamp > 10000) {  // 10 seconds cooldown
                ESP_LOGI(TAG, "Fall cooldown ended, resetting");
                fall_detection_reset();
            }
            break;
    }
    
    // Update result state
    s_result.state = s_current_state;
    
    return ESP_OK;
}

// Get current fall detection result
fall_result_t fall_detection_get_result(void)
{
    return s_result;
}

// Convert state to string
const char* fall_state_to_string(fall_state_t state)
{
    switch (state) {
        case FALL_STATE_NORMAL:    return "NORMAL";
        case FALL_STATE_FREE_FALL: return "FREE_FALL";
        case FALL_STATE_IMPACT:    return "IMPACT";
        case FALL_STATE_TILT:      return "TILT";
        case FALL_STATE_FALL:      return "FALL";
        default:                   return "UNKNOWN";
    }
}