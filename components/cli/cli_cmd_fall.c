#include "esp_console.h"
#include "fall_detection.h"
#include "mpu_manager.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static int cmd_fall_status(int argc, char **argv)
{
    fall_result_t result = fall_detection_get_result();
    const fall_orientation_t *orient = fall_detection_get_orientation();

    printf("\n========================================\n");
    printf("      Fall Detection Status            \n");
    printf("========================================\n");
    printf("\nCurrent State: %s\n", fall_state_to_string(result.state));
    printf("Fall Detected: %s\n", result.fall_detected ? "YES" : "NO");

    if (result.fall_detected)
    {
        printf("\n--- Fall Details ---\n");
        printf("  Time: %lu ms\n", result.fall_timestamp);
        printf("  Min Acceleration (Free fall): %.2fg\n", result.min_accel);
        printf("  Max Acceleration (Impact): %.2fg\n", result.max_accel);
        printf("  Final Tilt: %.1f°\n", result.final_tilt);
        printf("  Reason: %s\n", result.detection_reason);
    }

    printf("\n--- Orientation (Auto-calibrated) ---\n");
    if (orient->calibrated)
    {
        printf("  Status: CALIBRATED\n");
        printf("  Normal Z: %.2fg\n", orient->accel_z_normal);
        printf("  Pitch offset: %.1f°\n", orient->pitch_offset);
        printf("  Roll offset: %.1f°\n", orient->roll_offset);
        printf("  Note: System auto-calibrated for current wearing position\n");
    }
    else
    {
        printf("  Status: CALIBRATING...\n");
        printf("  Please keep device still for 10 seconds\n");
        printf("  System will auto-calibrate automatically\n");
    }

    printf("\n--- Detection Thresholds ---\n");
    fall_config_t config = fall_get_default_config();
    printf("  Free Fall Threshold: %.2fg\n", config.free_fall_threshold);
    printf("  Impact Threshold: %.2fg\n", config.impact_threshold);
    printf("  Tilt Threshold: %.1f°\n", config.tilt_threshold);

    printf("\n========================================\n");

    return 0;
}

static int cmd_fall_reset(int argc, char **argv)
{
    fall_detection_reset();
    printf("Fall detection state reset\n");
    return 0;
}

static int cmd_fall_test(int argc, char **argv)
{
    printf("\n========================================\n");
    printf("      Fall Detection Test              \n");
    printf("========================================\n");
    printf("\nTo test fall detection:\n");
    printf("1. Let device auto-calibrate (keep still for 10s)\n");
    printf("2. Check status: fall_status\n");
    printf("3. Simulate a fall by:\n");
    printf("   - Quickly drop the device (free fall)\n");
    printf("   - Then catch it or let it hit a soft surface (impact)\n");
    printf("   - Finally, tilt it sideways (tilt)\n");
    printf("\nWatch the console output for fall detection messages!\n");
    printf("\nCurrent state: %s\n", fall_state_to_string(fall_detection_get_result().state));
    printf("\n========================================\n");

    return 0;
}

static int cmd_fall_calibrate(int argc, char **argv)
{
    printf("\n========================================\n");
    printf("      Manual Orientation Calibration   \n");
    printf("========================================\n");
    printf("\nPlease stand still and hold device normally!\n");
    printf("Calibrating in 3 seconds...\n");

    for (int i = 3; i > 0; i--)
    {
        printf("%d...\n", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("Calibrating... Keep still!\n");

    mpu6050_data_t data;
    if (mpu_manager_get_data(&data) == ESP_OK)
    {
        fall_detection_calibrate_orientation(&data);
        printf("\n✓ Manual calibration complete!\n");
        printf("  Normal Z: %.2fg\n", data.accel_z);
        printf("  Pitch offset: %.1f°\n", data.pitch);
        printf("  Roll offset: %.1f°\n", data.roll);
    }
    else
    {
        printf("ERROR: Failed to read MPU data!\n");
        return 1;
    }

    printf("\n========================================\n");
    return 0;
}

static int cmd_fall_orientation(int argc, char **argv)
{
    const fall_orientation_t *orient = fall_detection_get_orientation();

    printf("\n========================================\n");
    printf("      Orientation Status               \n");
    printf("========================================\n");

    if (orient->calibrated)
    {
        printf("Status: CALIBRATED\n");
        printf("  Normal Z: %.2fg\n", orient->accel_z_normal);
        printf("  Pitch offset: %.1f°\n", orient->pitch_offset);
        printf("  Roll offset: %.1f°\n", orient->roll_offset);
        printf("\nThe system has automatically calibrated for:\n");
        if (orient->accel_z_normal > 0.7f)
        {
            printf("  → Device worn normally (upright position)\n");
        }
        else if (orient->accel_z_normal < -0.7f)
        {
            printf("  → Device worn upside down\n");
        }
        else
        {
            printf("  → Device worn horizontally (e.g., on wrist)\n");
        }
    }
    else
    {
        printf("Status: NOT CALIBRATED YET\n");
        printf("The system is auto-calibrating...\n");
        printf("Please keep the device still for 10 seconds.\n");
        printf("No manual action needed!\n");
    }

    printf("\n========================================\n");
    return 0;
}

void cli_register_fall(void)
{
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
        .command = "fall_test",
        .help = "Instructions for testing fall detection",
        .func = cmd_fall_test,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_calibrate",
        .help = "Manually calibrate orientation (optional - system auto-calibrates)",
        .func = cmd_fall_calibrate,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "fall_orient",
        .help = "Show orientation calibration status",
        .func = cmd_fall_orientation,
    });
}