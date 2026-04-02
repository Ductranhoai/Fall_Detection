#include "esp_console.h"
#include "fall_detection.h"
#include <stdio.h>
#include <string.h>

static int cmd_fall_status(int argc, char **argv)
{
    fall_result_t result = fall_detection_get_result();
    
    printf("\n========================================\n");
    printf("      Fall Detection Status            \n");
    printf("========================================\n");
    printf("\nCurrent State: %s\n", fall_state_to_string(result.state));
    printf("Fall Detected: %s\n", result.fall_detected ? "YES" : "NO");
    
    if (result.fall_detected) {
        printf("\n--- Fall Details ---\n");
        printf("  Time: %lu ms\n", result.fall_timestamp);
        printf("  Min Acceleration (Free fall): %.2fg\n", result.min_accel);
        printf("  Max Acceleration (Impact): %.2fg\n", result.max_accel);
        printf("  Final Tilt: %.1f°\n", result.final_tilt);
        printf("  Reason: %s\n", result.detection_reason);
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
    printf("1. Hold the device normally (Z ~ 1g)\n");
    printf("2. Simulate a fall by:\n");
    printf("   - Quickly drop the device (free fall)\n");
    printf("   - Then catch it or let it hit a soft surface (impact)\n");
    printf("   - Finally, tilt it sideways (tilt)\n");
    printf("\nWatch the console output for fall detection messages!\n");
    printf("\nCurrent state: %s\n", fall_state_to_string(fall_detection_get_result().state));
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
}