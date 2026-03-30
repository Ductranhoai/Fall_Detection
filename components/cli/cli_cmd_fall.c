#include "esp_console.h"
#include "mpu6050.h"
#include "fall_detection.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static mpu6050_data_t sensor_data;
static bool monitoring_active = false;
static TaskHandle_t monitoring_task = NULL;

static void fall_monitoring_task(void *arg)
{
    system_config_t *sys_config = config_get();
    int delay_ms = 1000 / sys_config->fall.sample_rate_hz;

    while (monitoring_active)
    {
        if (mpu6050_read_all(&sensor_data) == ESP_OK)
        {
            fall_detection_process(&sensor_data);
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    monitoring_task = NULL;
    vTaskDelete(NULL);
}

static int cmd_fall_start(int argc, char **argv)
{
    if (monitoring_active)
    {
        printf("Fall detection already active\n");
        return 0;
    }

    monitoring_active = true;
    xTaskCreate(fall_monitoring_task, "fall_monitor", 4096, NULL, 5, &monitoring_task);
    printf("Fall detection started\n");
    return 0;
}

static int cmd_fall_stop(int argc, char **argv)
{
    if (!monitoring_active)
    {
        printf("Fall detection not active\n");
        return 0;
    }

    monitoring_active = false;
    printf("Fall detection stopped\n");
    return 0;
}

static int cmd_fall_config(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: fall_config <param> [value]\n");
        printf("Parameters:\n");
        printf("  show                    - Show current config\n");
        printf("  impact <value>          - Impact threshold (0.5-8.0 g)\n");
        printf("  orientation <value>     - Orientation threshold (10-90 deg)\n");
        printf("  inactivity <value>      - Inactivity period (0.5-10.0 sec)\n");
        printf("  timeout <value>         - Fall timeout (5-30 sec)\n");
        printf("  samplerate <value>      - Sample rate (10-200 Hz)\n");
        printf("  autostart <0|1>         - Auto start on boot\n");
        printf("  reset                   - Reset to defaults\n");
        return 1;
    }

    if (strcmp(argv[1], "show") == 0)
    {
        config_print();
        return 0;
    }

    if (strcmp(argv[1], "reset") == 0)
    {
        config_reset_to_default();
        printf("Configuration reset to defaults\n");
        return 0;
    }

    if (argc < 3)
    {
        printf("Missing value\n");
        return 1;
    }

    esp_err_t ret = ESP_OK; // Khởi tạo ret với giá trị mặc định
    float value = atof(argv[2]);

    if (strcmp(argv[1], "impact") == 0)
    {
        ret = config_set_fall_param("impact", value);
        if (ret == ESP_OK)
            printf("Impact threshold set to %.2f g\n", value);
    }
    else if (strcmp(argv[1], "orientation") == 0)
    {
        ret = config_set_fall_param("orientation", value);
        if (ret == ESP_OK)
            printf("Orientation threshold set to %.2f deg\n", value);
    }
    else if (strcmp(argv[1], "inactivity") == 0)
    {
        ret = config_set_fall_param("inactivity", value);
        if (ret == ESP_OK)
            printf("Inactivity period set to %.2f sec\n", value);
    }
    else if (strcmp(argv[1], "timeout") == 0)
    {
        ret = config_set_fall_param("timeout", value);
        if (ret == ESP_OK)
            printf("Fall timeout set to %.2f sec\n", value);
    }
    else if (strcmp(argv[1], "samplerate") == 0)
    {
        ret = config_set_fall_param("sample_rate", value);
        if (ret == ESP_OK)
            printf("Sample rate set to %.0f Hz\n", value);
    }
    else if (strcmp(argv[1], "autostart") == 0)
    {
        system_config_t *cfg = config_get();
        cfg->fall.auto_start = (int)value ? true : false;
        config_save();
        printf("Auto start set to %s\n", cfg->fall.auto_start ? "ON" : "OFF");
        ret = ESP_OK;
    }
    else
    {
        printf("Invalid parameter: %s\n", argv[1]);
        return 1;
    }

    if (ret != ESP_OK)
    {
        printf("Failed to set parameter\n");
        return 1;
    }

    // Update fall detection if running
    if (monitoring_active)
    {
        fall_detection_init((fall_config_t *)&config_get()->fall);
    }

    return 0;
}

static int cmd_fall_status(int argc, char **argv)
{
    fall_state_t state = fall_detection_get_state();
    system_config_t *config = config_get();

    printf("\n========== Fall Detection Status ==========\n");
    printf("  Active:          %s\n", monitoring_active ? "Yes" : "No");
    printf("  Auto start:      %s\n", config->fall.auto_start ? "Yes" : "No");
    printf("  Current state:   ");

    switch (state)
    {
    case STATE_NORMAL:
        printf("Normal\n");
        break;
    case STATE_FALL_DETECTED:
        printf("Impact Detected\n");
        break;
    case STATE_INACTIVE_CHECK:
        printf("Checking Inactivity\n");
        break;
    case STATE_FALL_CONFIRMED:
        printf("FALL CONFIRMED!\n");
        break;
    default:
        printf("Unknown\n");
        break;
    }

    printf("\n--- Active Configuration ---\n");
    printf("  Impact:          %.2f g\n", config->fall.impact_threshold);
    printf("  Orientation:     %.2f deg\n", config->fall.orientation_threshold);
    printf("  Inactivity:      %.2f sec\n", config->fall.inactivity_threshold);
    printf("  Timeout:         %.2f sec\n", config->fall.fall_timeout);
    printf("  Sample rate:     %d Hz\n", config->fall.sample_rate_hz);
    printf("===========================================\n");

    return 0;
}

static int cmd_fall_reset(int argc, char **argv)
{
    fall_detection_reset();
    printf("Fall detection state reset\n");
    return 0;
}

static int cmd_mpu_read(int argc, char **argv)
{
    mpu6050_data_t data;

    if (mpu6050_read_all(&data) != ESP_OK)
    {
        printf("Failed to read MPU6050\n");
        return 1;
    }

    printf("\nMPU6050 Sensor Data:\n");
    printf("  Accel: X=%7.2f g, Y=%7.2f g, Z=%7.2f g\n",
           data.accel_x, data.accel_y, data.accel_z);
    printf("  Gyro:  X=%7.2f deg/s, Y=%7.2f deg/s, Z=%7.2f deg/s\n",
           data.gyro_x, data.gyro_y, data.gyro_z);

    // Calculate magnitude
    float magnitude = sqrt(data.accel_x * data.accel_x +
                           data.accel_y * data.accel_y +
                           data.accel_z * data.accel_z);
    printf("  Magnitude:       %.2f g\n", magnitude);

    return 0;
}

void cli_register_fall(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_start",
        .help = "Start fall detection",
        .func = cmd_fall_start,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_stop",
        .help = "Stop fall detection",
        .func = cmd_fall_stop,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_config",
        .help = "Configure fall detection parameters",
        .func = cmd_fall_config,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_status",
        .help = "Show fall detection status",
        .func = cmd_fall_status,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_reset",
        .help = "Reset fall detection state",
        .func = cmd_fall_reset,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_read",
        .help = "Read MPU6050 sensor data",
        .func = cmd_mpu_read,
    });
}