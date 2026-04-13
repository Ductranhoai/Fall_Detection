// #include "esp_console.h"
// #include "mpu_manager.h"
// #include <stdio.h>
// #include <string.h>
// #include <stdlib.h>
// #include <math.h>
// // Global flag for continuous read mode
// static volatile bool continuous_read_active = false;
// static TaskHandle_t continuous_read_task_handle = NULL;

// // Continuous read task
// static void continuous_read_task_func(void *arg)
// {
//     mpu6050_data_t data;

//     printf("\n========================================\n");
//     printf("   Continuous MPU Read Mode (2s interval)\n");
//     printf("   Type 'mpu_stop' and press Enter to stop\n");
//     printf("========================================\n\n");

//     while (continuous_read_active)
//     {
//         if (mpu_manager_get_data(&data) == ESP_OK)
//         {
//             printf("\n-----------------------------------\n");
//             printf("Accel (g):        X=%7.3f  Y=%7.3f  Z=%7.3f\n",
//                    data.accel_x, data.accel_y, data.accel_z);
//             printf("Gyro (deg/s):     X=%7.2f  Y=%7.2f  Z=%7.2f\n",
//                    data.gyro_x, data.gyro_y, data.gyro_z);
//             printf("Angles:           Pitch=%7.2f°  Roll=%7.2f°\n", data.pitch, data.roll);
//             printf("Temp:             %.2f°C\n", data.temperature);

//             float accel_z_abs = (data.accel_z < 0) ? -data.accel_z : data.accel_z;
//             float pitch_abs = (data.pitch < 0) ? -data.pitch : data.pitch;
//             float roll_abs = (data.roll < 0) ? -data.roll : data.roll;

//             if (accel_z_abs < 0.5f)
//             {
//                 printf("  WARNING: Low Z-axis acceleration!\n");
//             }
//             else if (pitch_abs > 30.0f || roll_abs > 30.0f)
//             {
//                 printf("  WARNING: Device tilted!\n");
//             }
//             else
//             {
//                 printf(" Device is stable\n");
//             }

//             printf("--- Next update in 1 seconds (type 'mpu_stop' to stop) ---\n");
//         }
//         else
//         {
//             printf(" Failed to read MPU data\n");
//         }
//         for (int i = 0; i < 10 && continuous_read_active; i++)
//         {
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }
//     }

//     printf("\n Continuous read stopped.\n");
//     continuous_read_task_handle = NULL;
//     vTaskDelete(NULL);
// }

// // Command: mpu_read
// static int cmd_mpu_read(int argc, char **argv)
// {
//     // Check for continuous mode
//     if (argc >= 2 && strcmp(argv[1], "-w") == 0)
//     {
//         if (!mpu_manager_is_initialized())
//         {
//             printf("MPU6050 not initialized!\n");
//             return 1;
//         }

//         // Check if already running
//         if (continuous_read_active)
//         {
//             printf("  Continuous read already running!\n");
//             printf("   Use 'mpu_stop' to stop it first\n");
//             return 1;
//         }

//         // Stop monitoring if running
//         if (mpu_manager_is_monitoring())
//         {
//             printf("Stopping background monitoring...\n");
//             mpu_manager_stop_monitoring();
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }

//         // Start continuous read
//         continuous_read_active = true;
//         xTaskCreate(continuous_read_task_func, "mpu_cont_read",
//                     4096, NULL, 5, &continuous_read_task_handle);

//         return 0;
//     }

//     // Normal single read
//     mpu6050_data_t data;

//     if (!mpu_manager_is_initialized())
//     {
//         printf("MPU6050 not initialized!\n");
//         return 1;
//     }

//     if (mpu_manager_get_data(&data) != ESP_OK)
//     {
//         printf("Failed to read MPU data\n");
//         return 1;
//     }

//     printf("\n========================================\n");
//     printf("         MPU6050 Sensor Data            \n");
//     printf("========================================\n");
//     printf("\n--- Accelerometer (g) ---\n");
//     printf("  X: %7.3f  Y: %7.3f  Z: %7.3f\n",
//            data.accel_x, data.accel_y, data.accel_z);

//     printf("\n--- Gyroscope (deg/s) ---\n");
//     printf("  X: %7.2f  Y: %7.2f  Z: %7.2f\n",
//            data.gyro_x, data.gyro_y, data.gyro_z);

//     printf("\n--- Orientation (degrees) ---\n");
//     printf("  Pitch: %7.2f°  Roll: %7.2f°\n", data.pitch, data.roll);

//     printf("\n--- Temperature ---\n");
//     printf("  Temperature: %.2f°C\n", data.temperature);

//     printf("\n========================================\n");

//     return 0;
// }

// // Command: mpu_stop - Stop continuous read
// static int cmd_mpu_stop(int argc, char **argv)
// {
//     if (continuous_read_active)
//     {
//         printf("\n Stopping continuous read...\n");
//         continuous_read_active = false;
//         vTaskDelay(pdMS_TO_TICKS(200));
//         printf(" Continuous read stopped\n");
//         return 0;
//     }
//     else if (mpu_manager_is_monitoring())
//     {
//         printf(" Stopping background monitoring...\n");
//         mpu_manager_stop_monitoring();
//         printf(" Background monitoring stopped\n");
//         return 0;
//     }
//     else
//     {
//         printf("No active MPU reading or monitoring\n");
//         printf("Available commands:\n");
//         printf("  mpu_read      - Read once\n");
//         printf("  mpu_read -w   - Start continuous reading\n");
//         printf("  mpu_monitor start - Start background monitoring\n");
//         return 1;
//     }
// }

// // Command: mpu_calibrate
// static int cmd_mpu_calibrate(int argc, char **argv)
// {
//     if (!mpu_manager_is_initialized())
//     {
//         printf("MPU6050 not initialized!\n");
//         return 1;
//     }

//     // Stop any active reading
//     if (continuous_read_active)
//     {
//         printf("Stopping continuous read mode...\n");
//         continuous_read_active = false;
//         vTaskDelay(pdMS_TO_TICKS(200));
//     }

//     if (mpu_manager_is_monitoring())
//     {
//         printf("Stopping background monitoring...\n");
//         mpu_manager_stop_monitoring();
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }

//     printf("\n========================================\n");
//     printf("      MPU6050 Calibration              \n");
//     printf("========================================\n");
//     printf("\nIMPORTANT: Place the device on a flat, stable surface!\n");
//     printf("Do NOT move the device during calibration.\n");
//     printf("\nStarting calibration in 3 seconds...\n");

//     for (int i = 3; i > 0; i--)
//     {
//         printf("%d...\n", i);
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }

//     printf("\nCalibrating... Keep device still!\n");

//     if (mpu6050_calibrate() != ESP_OK)
//     {
//         printf("Calibration failed!\n");
//         return 1;
//     }

//     printf("\n Calibration completed successfully!\n");
//     printf("New sensor biases have been applied.\n");
//     printf("\n========================================\n");

//     return 0;
// }

// // Command: mpu_monitor
// static int cmd_mpu_monitor(int argc, char **argv)
// {
//     if (!mpu_manager_is_initialized())
//     {
//         printf("MPU6050 not initialized!\n");
//         return 1;
//     }

//     if (argc < 2)
//     {
//         printf("Usage: mpu_monitor <start|stop|status>\n");
//         printf("  start  - Start continuous monitoring with callback\n");
//         printf("  stop   - Stop continuous monitoring\n");
//         printf("  status - Show monitoring status\n");
//         return 1;
//     }

//     if (strcmp(argv[1], "start") == 0)
//     {
//         // Stop continuous read if running
//         if (continuous_read_active)
//         {
//             printf("Stopping continuous read mode...\n");
//             continuous_read_active = false;
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }

//         mpu_manager_start_monitoring(NULL);
//         printf(" MPU monitoring started. Data will be logged to console.\n");
//         printf("Use 'mpu_stop' or 'mpu_monitor stop' to stop.\n");
//     }
//     else if (strcmp(argv[1], "stop") == 0)
//     {
//         mpu_manager_stop_monitoring();
//         printf("MPU monitoring stopped.\n");
//     }
//     else if (strcmp(argv[1], "status") == 0)
//     {
//         printf("\n=== MPU Status ===\n");
//         if (mpu_manager_is_monitoring())
//         {
//             printf(" Background monitoring: ACTIVE\n");
//             const mpu_config_t *config = mpu_manager_get_config();
//             printf("   Read interval: %lu ms\n", (unsigned long)config->read_interval_ms);
//         }
//         else
//         {
//             printf(" Background monitoring: INACTIVE\n");
//         }

//         if (continuous_read_active)
//         {
//             printf(" Continuous read mode: ACTIVE (2s interval)\n");
//             printf("   Use 'mpu_stop' to stop\n");
//         }
//         else
//         {
//             printf(" Continuous read mode: INACTIVE\n");
//         }
//         printf("==================\n\n");
//     }
//     else
//     {
//         printf("Invalid command. Use 'start', 'stop', or 'status'\n");
//         return 1;
//     }

//     return 0;
// }

// // Command: mpu_config
// static int cmd_mpu_config(int argc, char **argv)
// {
//     if (!mpu_manager_is_initialized())
//     {
//         printf("MPU6050 not initialized!\n");
//         return 1;
//     }

//     const mpu_config_t *config = mpu_manager_get_config();

//     printf("\n========================================\n");
//     printf("      MPU6050 Configuration             \n");
//     printf("========================================\n");
//     printf("\n--- I2C Configuration ---\n");
//     printf("  SDA Pin: GPIO%d\n", config->sda_pin);
//     printf("  SCL Pin: GPIO%d\n", config->scl_pin);
//     printf("  I2C Frequency: %lu Hz\n", (unsigned long)config->i2c_freq);

//     printf("\n--- Sensor Ranges ---\n");
//     printf("  Accelerometer: ");
//     switch (config->accel_range)
//     {
//     case MPU_ACCEL_RANGE_2G:
//         printf("±2g\n");
//         break;
//     case MPU_ACCEL_RANGE_4G:
//         printf("±4g\n");
//         break;
//     case MPU_ACCEL_RANGE_8G:
//         printf("±8g\n");
//         break;
//     case MPU_ACCEL_RANGE_16G:
//         printf("±16g\n");
//         break;
//     default:
//         printf("Unknown\n");
//     }

//     printf("  Gyroscope: ");
//     switch (config->gyro_range)
//     {
//     case MPU_GYRO_RANGE_250:
//         printf("±250°/s\n");
//         break;
//     case MPU_GYRO_RANGE_500:
//         printf("±500°/s\n");
//         break;
//     case MPU_GYRO_RANGE_1000:
//         printf("±1000°/s\n");
//         break;
//     case MPU_GYRO_RANGE_2000:
//         printf("±2000°/s\n");
//         break;
//     default:
//         printf("Unknown\n");
//     }

//     printf("\n--- Sampling ---\n");
//     printf("  Sample Rate: %lu Hz\n", (unsigned long)config->sample_rate_hz);
//     printf("  Read Interval: %lu ms\n", (unsigned long)config->read_interval_ms);

//     printf("\n--- Available Commands ---\n");
//     printf("  mpu_read        - Read sensor once\n");
//     printf("  mpu_read -w     - Start continuous reading (2s interval)\n");
//     printf("  mpu_stop        - Stop continuous reading or monitoring\n");
//     printf("  mpu_monitor     - Background monitoring with callback\n");
//     printf("  mpu_cal         - Calibrate sensor\n");
//     printf("  mpu_config      - Show this configuration\n");
//     printf("  mpu_test        - Quick hardware test\n");

//     printf("\n========================================\n");

//     return 0;
// }

// // Command: mpu_test
// static int cmd_mpu_test(int argc, char **argv)
// {
//     printf("\n========================================\n");
//     printf("      MPU6050 Quick Test               \n");
//     printf("========================================\n");

//     if (!mpu_manager_is_initialized())
//     {
//         printf(" MPU6050 NOT initialized!\n");
//         printf("Please check:\n");
//         printf("  - Hardware connections (SDA=GPIO21, SCL=GPIO22)\n");
//         printf("  - Power supply (3.3V)\n");
//         printf("  - I2C pull-up resistors (4.7kΩ)\n");
//         return 1;
//     }

//     printf(" MPU6050 initialized\n");

//     // Test read
//     mpu6050_data_t data;
//     if (mpu_manager_get_data(&data) == ESP_OK)
//     {
//         printf(" Sensor reading successful\n");
//         printf("   Accelerometer: (%.3f, %.3f, %.3f) g\n",
//                data.accel_x, data.accel_y, data.accel_z);
//         printf("   Temperature: %.2f°C\n", data.temperature);

//         // Verify if device is stable - Dùng phép tính trực tiếp, không dùng fabs
//         float z_diff = (data.accel_z - 1.0f);
//         if (z_diff < 0)
//             z_diff = -z_diff;

//         float x_abs = (data.accel_x < 0) ? -data.accel_x : data.accel_x;
//         float y_abs = (data.accel_y < 0) ? -data.accel_y : data.accel_y;

//         if (z_diff < 0.2f && x_abs < 0.1f && y_abs < 0.1f)
//         {
//             printf(" Device appears to be on a flat surface\n");
//         }
//         else
//         {
//             printf("  Device is not level or is moving\n");
//         }
//     }
//     else
//     {
//         printf(" Failed to read sensor data\n");
//         return 1;
//     }

//     printf("\n Status: MPU6050 is working properly\n");
//     printf("\n Quick start:\n");
//     printf("   mpu_read -w   - Start continuous reading\n");
//     printf("   mpu_stop      - Stop reading\n");
//     printf("   mpu_cal       - Calibrate sensor\n");

//     printf("\n========================================\n");

//     return 0;
// }

// // Register all MPU commands
// void cli_register_mpu(void)
// {
//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "mpu_read",
//         .help = "Read MPU6050 data. Use -w for continuous mode",
//         .func = cmd_mpu_read,
//     });

//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "mpu_stop",
//         .help = "Stop continuous reading or background monitoring",
//         .func = cmd_mpu_stop,
//     });

//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "mpu_cal",
//         .help = "Calibrate MPU6050 sensor (place on flat surface)",
//         .func = cmd_mpu_calibrate,
//     });

//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "mpu_monitor",
//         .help = "Background monitoring: mpu_monitor <start|stop|status>",
//         .func = cmd_mpu_monitor,
//     });

//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "mpu_config",
//         .help = "Show current MPU6050 configuration and commands",
//         .func = cmd_mpu_config,
//     });

//     esp_console_cmd_register(&(esp_console_cmd_t){
//         .command = "mpu_test",
//         .help = "Quick test MPU6050 functionality",
//         .func = cmd_mpu_test,
//     });
// }



#include "esp_console.h"
#include "mpu_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Global flag for continuous read mode
static volatile bool continuous_read_active = false;
static TaskHandle_t continuous_read_task_handle = NULL;

// Luu gia tri truoc do de phat hien chuyen dong
static struct {
    float prev_total_accel;
    float prev_pitch;
    float prev_roll;
    bool first_read;
} s_motion_state = {0, 0, 0, true};

// Ham khoi tao lai trang thai motion
static void reset_motion_state(void)
{
    s_motion_state.prev_total_accel = 0;
    s_motion_state.prev_pitch = 0;
    s_motion_state.prev_roll = 0;
    s_motion_state.first_read = true;
}

// Kiem tra do on dinh cua device bat ke huong deo
static bool is_device_stable(mpu6050_data_t *data)
{
    // Tinh tong gia toc (do lon vector) - khong phu thuoc huong
    float total_accel = sqrt(data->accel_x * data->accel_x + 
                             data->accel_y * data->accel_y + 
                             data->accel_z * data->accel_z);
    
    // Khi dung yen, tong gia toc phai xap xi 1g (0.9-1.1g)
    bool stable_magnitude = (total_accel > 0.85f && total_accel < 1.15f);
    
    // Kiem tra su thay doi giua cac lan doc
    bool stable_motion = true;
    bool stable_orientation = true;
    
    if (!s_motion_state.first_read) {
        float delta_accel = fabs(total_accel - s_motion_state.prev_total_accel);
        float delta_pitch = fabs(data->pitch - s_motion_state.prev_pitch);
        float delta_roll = fabs(data->roll - s_motion_state.prev_roll);
        
        stable_motion = (delta_accel < 0.1f);           // Thay doi gia toc < 0.1g
        stable_orientation = (delta_pitch < 5.0f && delta_roll < 5.0f); // Goc thay doi < 5 do
    }
    
    // Luu gia tri cho lan doc sau
    s_motion_state.prev_total_accel = total_accel;
    s_motion_state.prev_pitch = data->pitch;
    s_motion_state.prev_roll = data->roll;
    s_motion_state.first_read = false;
    
    return stable_magnitude && stable_motion && stable_orientation;
}

// Xac dinh huong deo hien tai
static const char* get_device_orientation(mpu6050_data_t *data)
{
    float abs_x = fabs(data->accel_x);
    float abs_y = fabs(data->accel_y);
    float abs_z = fabs(data->accel_z);
    
    // Tim truc co gia toc lon nhat (truc huong len/xuong)
    if (abs_z > abs_x && abs_z > abs_y) {
        if (data->accel_z > 0.7f) {
            return "HORIZONTAL (Chip UP) - Normal";
        } else if (data->accel_z < -0.7f) {
            return "HORIZONTAL (Chip DOWN) - INVERTED";
        }
    } else if (abs_x > abs_y) {
        if (data->accel_x > 0.7f) {
            return "VERTICAL (Right side up)";
        } else if (data->accel_x < -0.7f) {
            return "VERTICAL (Left side up)";
        }
    } else {
        if (data->accel_y > 0.7f) {
            return "VERTICAL (Forward)";
        } else if (data->accel_y < -0.7f) {
            return "VERTICAL (Backward)";
        }
    }
    
    return "UNKNOWN (Device moving)";
}

// Continuous read task
static void continuous_read_task_func(void *arg)
{
    mpu6050_data_t data;
    reset_motion_state();
    
    printf("\n");
    printf("====================================================================\n");
    printf("           CONTINUOUS MPU READ MODE (1s interval)                  \n");
    printf("         Type 'mpu_stop' and press Enter to stop                   \n");
    printf("====================================================================\n\n");

    while (continuous_read_active)
    {
        if (mpu_manager_get_data(&data) == ESP_OK)
        {
            printf("\n+------------------------------------------------------------------+\n");
            
            // Accelerometer data
            printf(" ACCELEROMETER (g)                                            \n");
            printf("    X: %7.3f     Y: %7.3f     Z: %7.3f                  \n",
                   data.accel_x, data.accel_y, data.accel_z);
            
            // Gyroscope data
            printf(" GYROSCOPE (deg/s)                                            \n");
            printf("    X: %7.2f     Y: %7.2f     Z: %7.2f                  \n",
                   data.gyro_x, data.gyro_y, data.gyro_z);
            
            // Angles
            printf(" ANGLES (degrees)                                             \n");
            printf("    Pitch: %7.2f     Roll: %7.2f                         \n", 
                   data.pitch, data.roll);
            
            // Temperature
            printf(" TEMPERATURE                                                  \n");
            printf("    %.2f degC                                                \n", data.temperature);
            
            // Total acceleration
            float total_accel = sqrt(data.accel_x * data.accel_x + 
                                     data.accel_y * data.accel_y + 
                                     data.accel_z * data.accel_z);
            printf(" TOTAL ACCELERATION                                           \n");
            printf("    %.3fg (expected: ~1.0g when still)                   \n", total_accel);
            
            // Stability check
            printf(" STABILITY CHECK                                              \n");
            if (is_device_stable(&data)) {
                printf("    [OK] Device is STABLE                                     \n");
            } else {
                printf("    [WARN] Device is MOVING or UNSTABLE!                      \n");
                if (total_accel < 0.7f) {
                    printf("       -> Free fall condition detected!                        \n");
                } else if (total_accel > 1.3f) {
                    printf("       -> Impact or strong acceleration!                       \n");
                } else {
                    printf("       -> Orientation changing                                 \n");
                }
            }
            
            // Orientation
            printf(" ORIENTATION                                                  \n");
            printf("    %s%s\n", 
                   get_device_orientation(&data),
                   strlen(get_device_orientation(&data)) > 35 ? "" : "                    ");
            
            // Warning for inverted orientation
            if (data.accel_z < -0.5f) {
                printf(" [WARN] Device appears INVERTED (upside down)!                 \n");
                printf("    Fall detection may need calibration                        \n");
            }
            
            printf("+------------------------------------------------------------------+\n");
            printf("  Next update in 1 second... (type 'mpu_stop' to stop)\n");
        }
        else
        {
            printf("[ERROR] Failed to read MPU data\n");
        }
        
        // Delay 1 second but check for stop flag every 100ms
        for (int i = 0; i < 10 && continuous_read_active; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    printf("\n[OK] Continuous read stopped.\n");
    continuous_read_task_handle = NULL;
    vTaskDelete(NULL);
}

// Command: mpu_read
static int cmd_mpu_read(int argc, char **argv)
{
    // Check for continuous mode
    if (argc >= 2 && strcmp(argv[1], "-w") == 0)
    {
        if (!mpu_manager_is_initialized())
        {
            printf("[ERROR] MPU6050 not initialized!\n");
            return 1;
        }

        // Check if already running
        if (continuous_read_active)
        {
            printf("[WARN] Continuous read already running!\n");
            printf("   Use 'mpu_stop' to stop it first\n");
            return 1;
        }

        // Stop monitoring if running
        if (mpu_manager_is_monitoring())
        {
            printf("Stopping background monitoring...\n");
            mpu_manager_stop_monitoring();
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // Start continuous read
        continuous_read_active = true;
        xTaskCreate(continuous_read_task_func, "mpu_cont_read",
                    4096, NULL, 5, &continuous_read_task_handle);

        return 0;
    }

    // Normal single read
    mpu6050_data_t data;

    if (!mpu_manager_is_initialized())
    {
        printf("[ERROR] MPU6050 not initialized!\n");
        return 1;
    }

    if (mpu_manager_get_data(&data) != ESP_OK)
    {
        printf("[ERROR] Failed to read MPU data\n");
        return 1;
    }

    float total_accel = sqrt(data.accel_x * data.accel_x + 
                             data.accel_y * data.accel_y + 
                             data.accel_z * data.accel_z);

    printf("\n");
    printf("====================================================================\n");
    printf("                    MPU6050 SENSOR DATA                            \n");
    printf("====================================================================\n");
    
    printf("\n+--------------------- ACCELEROMETER (g) ------------------------+\n");
    printf("  X: %7.3f     Y: %7.3f     Z: %7.3f                   \n",
           data.accel_x, data.accel_y, data.accel_z);
    
    printf("\n+--------------------- GYROSCOPE (deg/s) ------------------------+\n");
    printf("  X: %7.2f     Y: %7.2f     Z: %7.2f                   \n",
           data.gyro_x, data.gyro_y, data.gyro_z);
    
    printf("\n+--------------------- ORIENTATION (degrees) --------------------+\n");
    printf("  Pitch: %7.2f     Roll: %7.2f                            \n", data.pitch, data.roll);
    
    printf("\n+--------------------- OTHER ------------------------------------+\n");
    printf("  Temperature: %.2f degC                                      \n", data.temperature);
    printf("  Total Accel: %.3fg (should be ~1.0g)                       \n", total_accel);
    printf("  Orientation: %s%s\n", get_device_orientation(&data),
           strlen(get_device_orientation(&data)) > 35 ? "" : "                     ");
    
    if (data.accel_z < -0.5f) {
        printf("\n[WARN] Device appears to be INVERTED (upside down)!\n");
    }
    
    printf("\n====================================================================\n");
    printf("  Tips: Use 'mpu_read -w' for continuous reading                   \n");
    printf("        Use 'mpu_orient' for detailed orientation test             \n");
    printf("====================================================================\n");

    return 0;
}

// Command: mpu_stop
static int cmd_mpu_stop(int argc, char **argv)
{
    if (continuous_read_active)
    {
        printf("\n[STOP] Stopping continuous read...\n");
        continuous_read_active = false;
        vTaskDelay(pdMS_TO_TICKS(200));
        printf("[OK] Continuous read stopped\n");
        return 0;
    }
    else if (mpu_manager_is_monitoring())
    {
        printf("[STOP] Stopping background monitoring...\n");
        mpu_manager_stop_monitoring();
        printf("[OK] Background monitoring stopped\n");
        return 0;
    }
    else
    {
        printf("[INFO] No active MPU reading or monitoring\n");
        printf("\nAvailable commands:\n");
        printf("  mpu_read      - Read once\n");
        printf("  mpu_read -w   - Start continuous reading\n");
        printf("  mpu_monitor start - Start background monitoring\n");
        printf("  mpu_orient    - Test orientation\n");
        return 1;
    }
}

// Command: mpu_calibrate
static int cmd_mpu_calibrate(int argc, char **argv)
{
    if (!mpu_manager_is_initialized())
    {
        printf("[ERROR] MPU6050 not initialized!\n");
        return 1;
    }

    // Stop any active reading
    if (continuous_read_active)
    {
        printf("Stopping continuous read mode...\n");
        continuous_read_active = false;
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (mpu_manager_is_monitoring())
    {
        printf("Stopping background monitoring...\n");
        mpu_manager_stop_monitoring();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    printf("\n");
    printf("====================================================================\n");
    printf("                    MPU6050 CALIBRATION                            \n");
    printf("====================================================================\n");
    printf("\nIMPORTANT:\n");
    printf("  - Place the device on a FLAT, STABLE surface!\n");
    printf("  - DO NOT move the device during calibration\n");
    printf("  - Make sure the device is NOT inverted\n");
    printf("\nStarting calibration in 3 seconds...\n");

    for (int i = 3; i > 0; i--)
    {
        printf("  %d...\n", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("\n[CALIB] Calibrating... Keep device absolutely still!\n");

    if (mpu6050_calibrate() != ESP_OK)
    {
        printf("[ERROR] Calibration failed!\n");
        return 1;
    }

    printf("\n[OK] Calibration completed successfully!\n");
    printf("  New sensor biases have been applied.\n");
    reset_motion_state();
    
    printf("\n====================================================================\n");
    printf("  Tip: Use 'mpu_read' to verify calibration                        \n");
    printf("====================================================================\n");

    return 0;
}

// Command: mpu_monitor
static int cmd_mpu_monitor(int argc, char **argv)
{
    if (!mpu_manager_is_initialized())
    {
        printf("[ERROR] MPU6050 not initialized!\n");
        return 1;
    }

    if (argc < 2)
    {
        printf("Usage: mpu_monitor <start|stop|status>\n");
        printf("  start  - Start continuous monitoring with fall detection\n");
        printf("  stop   - Stop continuous monitoring\n");
        printf("  status - Show monitoring status\n");
        return 1;
    }

    if (strcmp(argv[1], "start") == 0)
    {
        // Stop continuous read if running
        if (continuous_read_active)
        {
            printf("Stopping continuous read mode...\n");
            continuous_read_active = false;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        mpu_manager_start_monitoring(NULL);
        printf("[OK] MPU monitoring started.\n");
        printf("  - Fall detection is ACTIVE\n");
        printf("  - Data will be logged to console\n");
        printf("  - Use 'mpu_stop' or 'mpu_monitor stop' to stop\n");
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        mpu_manager_stop_monitoring();
        printf("[OK] MPU monitoring stopped.\n");
    }
    else if (strcmp(argv[1], "status") == 0)
    {
        printf("\n====================================================================\n");
        printf("                      MPU STATUS                                  \n");
        printf("====================================================================\n");
        
        if (mpu_manager_is_monitoring())
        {
            printf("  Background monitoring: [ACTIVE]\n");
            const mpu_config_t *config = mpu_manager_get_config();
            printf("    Read interval: %lu ms\n", (unsigned long)config->read_interval_ms);
            printf("    Fall detection: ENABLED\n");
        }
        else
        {
            printf("  Background monitoring: [INACTIVE]\n");
        }

        if (continuous_read_active)
        {
            printf("  Continuous read mode: [ACTIVE] (1s interval)\n");
            printf("    Use 'mpu_stop' to stop\n");
        }
        else
        {
            printf("  Continuous read mode: [INACTIVE]\n");
        }
        
        printf("  MPU Initialized: %s\n", mpu_manager_is_initialized() ? "[YES]" : "[NO]");
        printf("====================================================================\n");
    }
    else
    {
        printf("[ERROR] Invalid command. Use 'start', 'stop', or 'status'\n");
        return 1;
    }

    return 0;
}

// Command: mpu_orientation (NEW - Chi tiet ve huong deo)
static int cmd_mpu_orientation(int argc, char **argv)
{
    if (!mpu_manager_is_initialized())
    {
        printf("[ERROR] MPU6050 not initialized!\n");
        return 1;
    }

    mpu6050_data_t data;
    if (mpu_manager_get_data(&data) != ESP_OK)
    {
        printf("[ERROR] Failed to read MPU data\n");
        return 1;
    }

    float total_accel = sqrt(data.accel_x * data.accel_x + 
                             data.accel_y * data.accel_y + 
                             data.accel_z * data.accel_z);
    
    float abs_x = fabs(data.accel_x);
    float abs_y = fabs(data.accel_y);
    float abs_z = fabs(data.accel_z);

    printf("\n");
    printf("====================================================================\n");
    printf("                 DEVICE ORIENTATION TEST                           \n");
    printf("====================================================================\n");
    
    printf("\n+--------------------- ACCELERATION VECTOR -----------------------+\n");
    printf("|  X: %7.3fg     Y: %7.3fg     Z: %7.3fg                 |\n", 
           data.accel_x, data.accel_y, data.accel_z);
    printf("|  Magnitude: %.3fg (should be ~1.0g)                           |\n", total_accel);
    
    printf("\n+--------------------- PRIMARY GRAVITY AXIS ---------------------+\n");
    if (abs_z > abs_x && abs_z > abs_y) {
        printf("|  Main axis: Z-AXIS (%.3fg)                                      |\n", abs_z);
        if (data.accel_z > 0.7f) {
            printf("|  Direction: POSITIVE (Chip facing UP)                          |\n");
            printf("|  Status: [OK] NORMAL orientation                              |\n");
        } else if (data.accel_z < -0.7f) {
            printf("|  Direction: NEGATIVE (Chip facing DOWN)                        |\n");
            printf("|  Status: [WARN] INVERTED orientation!                          |\n");
        }
    } else if (abs_x > abs_y) {
        printf("|  Main axis: X-AXIS (%.3fg)                                      |\n", abs_x);
        if (data.accel_x > 0.7f) {
            printf("|  Direction: POSITIVE (Right side up)                           |\n");
        } else if (data.accel_x < -0.7f) {
            printf("|  Direction: NEGATIVE (Left side up)                            |\n");
        }
        printf("|  Status: Device is VERTICAL                                    |\n");
    } else {
        printf("|  Main axis: Y-AXIS (%.3fg)                                      |\n", abs_y);
        if (data.accel_y > 0.7f) {
            printf("|  Direction: POSITIVE (Forward)                                 |\n");
        } else if (data.accel_y < -0.7f) {
            printf("|  Direction: NEGATIVE (Backward)                                |\n");
        }
        printf("|  Status: Device is VERTICAL                                    |\n");
    }
    
    printf("\n+--------------------- ANGLES ------------------------------------+\n");
    printf("|  Pitch: %7.2f (rotation around X-axis)                        |\n", data.pitch);
    printf("|  Roll:  %7.2f (rotation around Y-axis)                        |\n", data.roll);
    
    printf("\n+--------------------- RECOMMENDATIONS --------------------------+\n");
    if (data.accel_z < -0.5f) {
        printf("|  [WARN] WARNING: Device is INVERTED!                            |\n");
        printf("|  -> For accurate fall detection:                               |\n");
        printf("|     1. Re-mount device with chip facing UP                     |\n");
        printf("|     2. Or re-calibrate for inverted use                        |\n");
    } else if (abs_x > abs_z || abs_y > abs_z) {
        printf("|  [INFO] Device is mounted VERTICALLY                            |\n");
        printf("|  -> Fall detection works but may need                          |\n");
        printf("|     threshold adjustments                                      |\n");
    } else {
        printf("|  [OK] Device orientation is NORMAL                             |\n");
        printf("|  -> Fall detection ready to use                                |\n");
    }
    
    printf("\n====================================================================\n");
    printf("  Tip: Keep device still and run again for stable reading          \n");
    printf("====================================================================\n");

    return 0;
}

// Command: mpu_config
static int cmd_mpu_config(int argc, char **argv)
{
    if (!mpu_manager_is_initialized())
    {
        printf("[ERROR] MPU6050 not initialized!\n");
        return 1;
    }

    const mpu_config_t *config = mpu_manager_get_config();

    printf("\n");
    printf("====================================================================\n");
    printf("                   MPU6050 CONFIGURATION                           \n");
    printf("====================================================================\n");
    
    printf("\n+--------------------- I2C CONFIGURATION -------------------------+\n");
    printf("|  SDA Pin:      GPIO%d                                          |\n", config->sda_pin);
    printf("|  SCL Pin:      GPIO%d                                          |\n", config->scl_pin);
    printf("|  I2C Port:     %d                                              |\n", config->i2c_port);
    printf("|  I2C Frequency: %lu Hz                                         |\n", (unsigned long)config->i2c_freq);

    printf("\n+--------------------- SENSOR RANGES ----------------------------+\n");
    printf("|  Accelerometer: ");
    switch (config->accel_range)
    {
    case MPU_ACCEL_RANGE_2G:
        printf("+-2g (best sensitivity)                |\n");
        break;
    case MPU_ACCEL_RANGE_4G:
        printf("+-4g                                  |\n");
        break;
    case MPU_ACCEL_RANGE_8G:
        printf("+-8g                                  |\n");
        break;
    case MPU_ACCEL_RANGE_16G:
        printf("+-16g (highest range)                 |\n");
        break;
    default:
        printf("Unknown                               |\n");
    }

    printf("|  Gyroscope:     ");
    switch (config->gyro_range)
    {
    case MPU_GYRO_RANGE_250:
        printf("+-250 deg/s (best sensitivity)        |\n");
        break;
    case MPU_GYRO_RANGE_500:
        printf("+-500 deg/s                           |\n");
        break;
    case MPU_GYRO_RANGE_1000:
        printf("+-1000 deg/s                          |\n");
        break;
    case MPU_GYRO_RANGE_2000:
        printf("+-2000 deg/s (highest range)          |\n");
        break;
    default:
        printf("Unknown                               |\n");
    }

    printf("\n+--------------------- SAMPLING ---------------------------------+\n");
    printf("|  Sample Rate:  %lu Hz                                          |\n", (unsigned long)config->sample_rate_hz);
    printf("|  Read Interval: %lu ms                                         |\n", (unsigned long)config->read_interval_ms);
    printf("|  Calibration:  %s                                             |\n", config->enable_calibration ? "ENABLED" : "DISABLED");

    printf("\n+--------------------- AVAILABLE COMMANDS -----------------------+\n");
    printf("|  mpu_read        - Read sensor once                           |\n");
    printf("|  mpu_read -w     - Continuous reading                         |\n");
    printf("|  mpu_stop        - Stop reading/monitoring                    |\n");
    printf("|  mpu_monitor     - Background monitoring                      |\n");
    printf("|  mpu_cal         - Calibrate sensor                           |\n");
    printf("|  mpu_orient      - Test orientation (NEW!)                    |\n");
    printf("|  mpu_config      - Show this config                           |\n");
    printf("|  mpu_test        - Quick hardware test                        |\n");

    printf("\n====================================================================\n");
    printf("  Tip: Run 'mpu_orient' to check device mounting orientation      \n");
    printf("====================================================================\n");

    return 0;
}

// Command: mpu_test
static int cmd_mpu_test(int argc, char **argv)
{
    printf("\n");
    printf("====================================================================\n");
    printf("                   MPU6050 QUICK TEST                              \n");
    printf("====================================================================\n");

    if (!mpu_manager_is_initialized())
    {
        printf("\n[ERROR] MPU6050 NOT initialized!\n");
        printf("\nPlease check:\n");
        printf("  [X] Hardware connections (SDA=GPIO21, SCL=GPIO22)\n");
        printf("  [X] Power supply (3.3V)\n");
        printf("  [X] I2C pull-up resistors (4.7k Ohm)\n");
        return 1;
    }

    printf("\n[OK] MPU6050 initialized\n");

    // Test read
    mpu6050_data_t data;
    if (mpu_manager_get_data(&data) == ESP_OK)
    {
        printf("[OK] Sensor reading successful\n");
        printf("\n  Current values:\n");
        printf("    Accelerometer: (%.3f, %.3f, %.3f) g\n",
               data.accel_x, data.accel_y, data.accel_z);
        printf("    Temperature: %.2f degC\n", data.temperature);
        
        float total_accel = sqrt(data.accel_x * data.accel_x +
                                 data.accel_y * data.accel_y +
                                 data.accel_z * data.accel_z);
        printf("    Total acceleration: %.3fg\n", total_accel);

        // Verify if device is stable - using improved method
        if (total_accel > 0.9f && total_accel < 1.1f) {
            printf("\n[OK] Device appears to be STABLE\n");
            printf("   Orientation: %s\n", get_device_orientation(&data));
        } else {
            printf("\n[WARN] Device is MOVING or NOT LEVEL\n");
            printf("   Total acceleration should be ~1.0g when still\n");
        }
        
        // Warning for inverted
        if (data.accel_z < -0.5f) {
            printf("\n[WARN] WARNING: Device is INVERTED (upside down)!\n");
        }
    }
    else
    {
        printf("[ERROR] Failed to read sensor data\n");
        return 1;
    }

    printf("\n[OK] Status: MPU6050 is working properly\n");
    printf("\n+--------------------- QUICK START ------------------------------+\n");
    printf("|  mpu_read -w   - Start continuous reading                      |\n");
    printf("|  mpu_orient    - Check orientation                             |\n");
    printf("|  mpu_cal       - Calibrate sensor                              |\n");
    printf("|  mpu_stop      - Stop reading                                  |\n");
    printf("+----------------------------------------------------------------+\n");
    printf("\n====================================================================\n");

    return 0;
}

// Register all MPU commands
void cli_register_mpu(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_read",
        .help = "Read MPU6050 data. Use -w for continuous mode",
        .func = cmd_mpu_read,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_stop",
        .help = "Stop continuous reading or background monitoring",
        .func = cmd_mpu_stop,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_cal",
        .help = "Calibrate MPU6050 sensor (place on flat surface)",
        .func = cmd_mpu_calibrate,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_monitor",
        .help = "Background monitoring: mpu_monitor <start|stop|status>",
        .func = cmd_mpu_monitor,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_config",
        .help = "Show current MPU6050 configuration and commands",
        .func = cmd_mpu_config,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_test",
        .help = "Quick test MPU6050 functionality",
        .func = cmd_mpu_test,
    });

    // Register orientation test command
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "mpu_orient",
        .help = "Test device orientation and mounting position",
        .func = cmd_mpu_orientation,
    });
}