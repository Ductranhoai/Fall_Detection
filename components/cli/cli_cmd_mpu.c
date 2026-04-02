#include "esp_console.h"
#include "mpu_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>  
// Global flag for continuous read mode
static volatile bool continuous_read_active = false;
static TaskHandle_t continuous_read_task_handle = NULL;

// Continuous read task
static void continuous_read_task_func(void *arg)
{
    mpu6050_data_t data;
    
    printf("\n========================================\n");
    printf("   Continuous MPU Read Mode (2s interval)\n");
    printf("   Type 'mpu_stop' and press Enter to stop\n");
    printf("========================================\n\n");
    
    while (continuous_read_active) {
        if (mpu_manager_get_data(&data) == ESP_OK) {
            printf("\n--- Update ---\n");
            
            // Accelerometer
            printf("Accel (g): X=%7.3f  Y=%7.3f  Z=%7.3f\n", 
                   data.accel_x, data.accel_y, data.accel_z);
            
            // Gyroscope
            printf("Gyro (deg/s): X=%7.2f  Y=%7.2f  Z=%7.2f\n", 
                   data.gyro_x, data.gyro_y, data.gyro_z);
            
            // Orientation
            printf("Angles: Pitch=%7.2f°  Roll=%7.2f°\n", data.pitch, data.roll);
            
            // Temperature
            printf("Temp: %.2f°C\n", data.temperature);
            
            // Status - Dùng fabsf cho float
            float accel_z_abs = (data.accel_z < 0) ? -data.accel_z : data.accel_z;
            float pitch_abs = (data.pitch < 0) ? -data.pitch : data.pitch;
            float roll_abs = (data.roll < 0) ? -data.roll : data.roll;
            
            if (accel_z_abs < 0.5f) {
                printf("⚠️  WARNING: Low Z-axis acceleration!\n");
            } else if (pitch_abs > 30.0f || roll_abs > 30.0f) {
                printf("⚠️  WARNING: Device tilted!\n");
            } else {
                printf("✅ Device is stable\n");
            }
            
            printf("--- Next update in 2 seconds (type 'mpu_stop' to stop) ---\n");
        } else {
            printf("❌ Failed to read MPU data\n");
        }
        
        // Wait for 2 seconds
        for (int i = 0; i < 20 && continuous_read_active; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    printf("\n✅ Continuous read stopped.\n");
    continuous_read_task_handle = NULL;
    vTaskDelete(NULL);
}

// Command: mpu_read
static int cmd_mpu_read(int argc, char **argv)
{
    // Check for continuous mode
    if (argc >= 2 && strcmp(argv[1], "-w") == 0) {
        if (!mpu_manager_is_initialized()) {
            printf("MPU6050 not initialized!\n");
            return 1;
        }
        
        // Check if already running
        if (continuous_read_active) {
            printf("⚠️  Continuous read already running!\n");
            printf("   Use 'mpu_stop' to stop it first\n");
            return 1;
        }
        
        // Stop monitoring if running
        if (mpu_manager_is_monitoring()) {
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
    
    if (!mpu_manager_is_initialized()) {
        printf("MPU6050 not initialized!\n");
        return 1;
    }
    
    if (mpu_manager_get_data(&data) != ESP_OK) {
        printf("Failed to read MPU data\n");
        return 1;
    }
    
    printf("\n========================================\n");
    printf("         MPU6050 Sensor Data            \n");
    printf("========================================\n");
    printf("\n--- Accelerometer (g) ---\n");
    printf("  X: %7.3f  Y: %7.3f  Z: %7.3f\n", 
           data.accel_x, data.accel_y, data.accel_z);
    
    printf("\n--- Gyroscope (deg/s) ---\n");
    printf("  X: %7.2f  Y: %7.2f  Z: %7.2f\n", 
           data.gyro_x, data.gyro_y, data.gyro_z);
    
    printf("\n--- Orientation (degrees) ---\n");
    printf("  Pitch: %7.2f°  Roll: %7.2f°\n", data.pitch, data.roll);
    
    printf("\n--- Temperature ---\n");
    printf("  Temperature: %.2f°C\n", data.temperature);
    
    printf("\n--- Usage ---\n");
    printf("  Continuous reading: mpu_read -w\n");
    printf("  Stop continuous: mpu_stop\n");
    
    printf("\n========================================\n");
    
    return 0;
}

// Command: mpu_stop - Stop continuous read
static int cmd_mpu_stop(int argc, char **argv)
{
    if (continuous_read_active) {
        printf("\n🛑 Stopping continuous read...\n");
        continuous_read_active = false;
        vTaskDelay(pdMS_TO_TICKS(200));
        printf("✅ Continuous read stopped\n");
        return 0;
    } 
    else if (mpu_manager_is_monitoring()) {
        printf("🛑 Stopping background monitoring...\n");
        mpu_manager_stop_monitoring();
        printf("✅ Background monitoring stopped\n");
        return 0;
    }
    else {
        printf("No active MPU reading or monitoring\n");
        printf("Available commands:\n");
        printf("  mpu_read      - Read once\n");
        printf("  mpu_read -w   - Start continuous reading\n");
        printf("  mpu_monitor start - Start background monitoring\n");
        return 1;
    }
}

// Command: mpu_calibrate
static int cmd_mpu_calibrate(int argc, char **argv)
{
    if (!mpu_manager_is_initialized()) {
        printf("MPU6050 not initialized!\n");
        return 1;
    }
    
    // Stop any active reading
    if (continuous_read_active) {
        printf("Stopping continuous read mode...\n");
        continuous_read_active = false;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    if (mpu_manager_is_monitoring()) {
        printf("Stopping background monitoring...\n");
        mpu_manager_stop_monitoring();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    printf("\n========================================\n");
    printf("      MPU6050 Calibration              \n");
    printf("========================================\n");
    printf("\nIMPORTANT: Place the device on a flat, stable surface!\n");
    printf("Do NOT move the device during calibration.\n");
    printf("\nStarting calibration in 3 seconds...\n");
    
    for (int i = 3; i > 0; i--) {
        printf("%d...\n", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    printf("\nCalibrating... Keep device still!\n");
    
    if (mpu6050_calibrate() != ESP_OK) {
        printf("Calibration failed!\n");
        return 1;
    }
    
    printf("\n✅ Calibration completed successfully!\n");
    printf("New sensor biases have been applied.\n");
    printf("\n========================================\n");
    
    return 0;
}

// Command: mpu_monitor
static int cmd_mpu_monitor(int argc, char **argv)
{
    if (!mpu_manager_is_initialized()) {
        printf("MPU6050 not initialized!\n");
        return 1;
    }
    
    if (argc < 2) {
        printf("Usage: mpu_monitor <start|stop|status>\n");
        printf("  start  - Start continuous monitoring with callback\n");
        printf("  stop   - Stop continuous monitoring\n");
        printf("  status - Show monitoring status\n");
        return 1;
    }
    
    if (strcmp(argv[1], "start") == 0) {
        // Stop continuous read if running
        if (continuous_read_active) {
            printf("Stopping continuous read mode...\n");
            continuous_read_active = false;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        mpu_manager_start_monitoring(NULL);
        printf("✅ MPU monitoring started. Data will be logged to console.\n");
        printf("Use 'mpu_stop' or 'mpu_monitor stop' to stop.\n");
    } 
    else if (strcmp(argv[1], "stop") == 0) {
        mpu_manager_stop_monitoring();
        printf("MPU monitoring stopped.\n");
    }
    else if (strcmp(argv[1], "status") == 0) {
        printf("\n=== MPU Status ===\n");
        if (mpu_manager_is_monitoring()) {
            printf("✅ Background monitoring: ACTIVE\n");
            const mpu_config_t *config = mpu_manager_get_config();
            printf("   Read interval: %lu ms\n", (unsigned long)config->read_interval_ms);
        } else {
            printf("❌ Background monitoring: INACTIVE\n");
        }
        
        if (continuous_read_active) {
            printf("✅ Continuous read mode: ACTIVE (2s interval)\n");
            printf("   Use 'mpu_stop' to stop\n");
        } else {
            printf("❌ Continuous read mode: INACTIVE\n");
        }
        printf("==================\n\n");
    }
    else {
        printf("Invalid command. Use 'start', 'stop', or 'status'\n");
        return 1;
    }
    
    return 0;
}

// Command: mpu_config
static int cmd_mpu_config(int argc, char **argv)
{
    if (!mpu_manager_is_initialized()) {
        printf("MPU6050 not initialized!\n");
        return 1;
    }
    
    const mpu_config_t *config = mpu_manager_get_config();
    
    printf("\n========================================\n");
    printf("      MPU6050 Configuration             \n");
    printf("========================================\n");
    printf("\n--- I2C Configuration ---\n");
    printf("  SDA Pin: GPIO%d\n", config->sda_pin);
    printf("  SCL Pin: GPIO%d\n", config->scl_pin);
    printf("  I2C Frequency: %lu Hz\n", (unsigned long)config->i2c_freq);
    
    printf("\n--- Sensor Ranges ---\n");
    printf("  Accelerometer: ");
    switch(config->accel_range) {
        case MPU_ACCEL_RANGE_2G:  printf("±2g\n"); break;
        case MPU_ACCEL_RANGE_4G:  printf("±4g\n"); break;
        case MPU_ACCEL_RANGE_8G:  printf("±8g\n"); break;
        case MPU_ACCEL_RANGE_16G: printf("±16g\n"); break;
        default: printf("Unknown\n");
    }
    
    printf("  Gyroscope: ");
    switch(config->gyro_range) {
        case MPU_GYRO_RANGE_250:  printf("±250°/s\n"); break;
        case MPU_GYRO_RANGE_500:  printf("±500°/s\n"); break;
        case MPU_GYRO_RANGE_1000: printf("±1000°/s\n"); break;
        case MPU_GYRO_RANGE_2000: printf("±2000°/s\n"); break;
        default: printf("Unknown\n");
    }
    
    printf("\n--- Sampling ---\n");
    printf("  Sample Rate: %lu Hz\n", (unsigned long)config->sample_rate_hz);
    printf("  Read Interval: %lu ms\n", (unsigned long)config->read_interval_ms);
    
    printf("\n--- Available Commands ---\n");
    printf("  mpu_read        - Read sensor once\n");
    printf("  mpu_read -w     - Start continuous reading (2s interval)\n");
    printf("  mpu_stop        - Stop continuous reading or monitoring\n");
    printf("  mpu_monitor     - Background monitoring with callback\n");
    printf("  mpu_cal         - Calibrate sensor\n");
    printf("  mpu_config      - Show this configuration\n");
    printf("  mpu_test        - Quick hardware test\n");
    
    printf("\n========================================\n");
    
    return 0;
}

// Command: mpu_test
static int cmd_mpu_test(int argc, char **argv)
{
    printf("\n========================================\n");
    printf("      MPU6050 Quick Test               \n");
    printf("========================================\n");
    
    if (!mpu_manager_is_initialized()) {
        printf("❌ MPU6050 NOT initialized!\n");
        printf("Please check:\n");
        printf("  - Hardware connections (SDA=GPIO21, SCL=GPIO22)\n");
        printf("  - Power supply (3.3V)\n");
        printf("  - I2C pull-up resistors (4.7kΩ)\n");
        return 1;
    }
    
    printf("✅ MPU6050 initialized\n");
    
    // Test read
    mpu6050_data_t data;
    if (mpu_manager_get_data(&data) == ESP_OK) {
        printf("✅ Sensor reading successful\n");
        printf("   Accelerometer: (%.3f, %.3f, %.3f) g\n", 
               data.accel_x, data.accel_y, data.accel_z);
        printf("   Temperature: %.2f°C\n", data.temperature);
        
        // Verify if device is stable - Dùng phép tính trực tiếp, không dùng fabs
        float z_diff = (data.accel_z - 1.0f);
        if (z_diff < 0) z_diff = -z_diff;
        
        float x_abs = (data.accel_x < 0) ? -data.accel_x : data.accel_x;
        float y_abs = (data.accel_y < 0) ? -data.accel_y : data.accel_y;
        
        if (z_diff < 0.2f && x_abs < 0.1f && y_abs < 0.1f) {
            printf("✅ Device appears to be on a flat surface\n");
        } else {
            printf("⚠️  Device is not level or is moving\n");
        }
    } else {
        printf("❌ Failed to read sensor data\n");
        return 1;
    }
    
    printf("\n📊 Status: MPU6050 is working properly\n");
    printf("\n💡 Quick start:\n");
    printf("   mpu_read -w   - Start continuous reading\n");
    printf("   mpu_stop      - Stop reading\n");
    printf("   mpu_cal       - Calibrate sensor\n");
    
    printf("\n========================================\n");
    
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
}