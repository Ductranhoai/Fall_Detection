#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_err.h"

// Fall detection configuration structure
typedef struct
{
    float impact_threshold;      // Impact threshold (g)
    float orientation_threshold; // Orientation change threshold (degrees)
    float inactivity_threshold;  // Inactivity period (seconds)
    float fall_timeout;          // Timeout for fall confirmation (seconds)
    int sample_rate_hz;          // Sample rate (Hz)
    bool auto_start;             // Auto-start fall detection on boot
} fall_detection_config_t;

// System configuration - CHỈ ĐỊNH NGHĨA MỘT LẦN
typedef struct
{
    fall_detection_config_t fall;
    i2c_config_t i2c;
    i2c_port_t i2c_port; // Thêm port riêng
    bool enable_log_to_file;
    char log_file_path[64];
} system_config_t;

// Function prototypes
esp_err_t config_init(void);
esp_err_t config_save(void);
esp_err_t config_load(void);
esp_err_t config_reset_to_default(void);
system_config_t *config_get(void);
esp_err_t config_set_fall_param(const char *param, float value);
esp_err_t config_set_i2c_param(const char *param, int value);
void config_print(void);