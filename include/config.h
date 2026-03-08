/**
 * @file config.h
 * @brief Global configuration file for the Fall Detection System
 */

#ifndef CONFIG_H
#define CONFIG_H

// ======================================================
// PROJECT INFORMATION
// ======================================================

#define PROJECT_NAME "Fall Detection System"
#define PROJECT_VERSION "1.0.0"

// ======================================================
// SYSTEM CONFIGURATION
// ======================================================

#define MAIN_LOOP_FREQ 100       /*!< Main loop frequency (Hz) */
#define FALL_TIME_WINDOW_MS 2000 /*!< Time window for fall detection */

// ======================================================
// HARDWARE PIN CONFIGURATION
// ======================================================

// ---------- I2C (MPU6050) ----------
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_MASTER_TIMEOUT_MS 1000

// ---------- GPS (UART2) ----------
#define GPS_UART_NUM UART_NUM_2
#define GPS_TX_PIN 17
#define GPS_RX_PIN 16
#define GPS_BAUD_RATE 9600
#define GPS_BUF_SIZE 1024

// ---------- SIM800L (UART1) ----------
#define SIM_UART_NUM UART_NUM_1
#define SIM_TX_PIN 18
#define SIM_RX_PIN 19
#define SIM_BAUD_RATE 115200
#define SIM_RESET_PIN 23

// ---------- Indicators ----------
#define BUZZER_PIN 25
#define LED_STATUS_PIN 2
#define LED_ALERT_PIN 26

// ---------- Pulse Sensor ----------
#define PULSE_SENSOR_PIN 34
#define ADC_ATTEN ADC_ATTEN_DB_11

// ---------- Battery Monitor ----------
#define BATTERY_ADC_PIN 35

// ======================================================
// SENSOR CONFIGURATION
// ======================================================

// ---------- MPU6050 ----------
#define MPU6050_ACCEL_RANGE 2
#define MPU6050_GYRO_RANGE 250
#define MPU6050_DLPF 42
#define MPU6050_SAMPLE_RATE 100

// ======================================================
// FALL DETECTION (3-PHASE ALGORITHM)
// ======================================================

// ---------- Free Fall ----------
#define FREE_FALL_THRESHOLD 0.3f
#define FREE_FALL_JERK_THRESHOLD 10.0f
#define IMPACT_JERK_THRESHOLD 5.0f
#define VERTICAL_IMPACT_RATIO 1.5f
#define POST_FALL_ACCEL_TOLERANCE 0.2f
#define POST_FALL_JERK_THRESHOLD 1.0f
#define HISTORY_CHECK_SIZE 5
#define HISTORY_CHECK_THRESHOLD 3

#define FREE_FALL_TIME_MIN 80
#define FREE_FALL_TIME_MAX 500

// ---------- Impact ----------
#define IMPACT_THRESHOLD 2.8f
#define IMPACT_WINDOW 300

// ---------- Post Fall ----------
#define POST_FALL_TIME 2000
#define ORIENTATION_THRESHOLD 45.0f

// ======================================================
// GYROSCOPE PARAMETERS
// ======================================================

#define GYRO_THRESHOLD 300.0f
#define GYRO_FALL_THRESHOLD 150.0f
#define GYRO_STABLE_THRESHOLD 20.0f

// ======================================================
// ROTATION DETECTION
// ======================================================

#define ROTATION_THRESHOLD 180.0f
#define ROTATION_TIME_MAX 1000

// ======================================================
// BODY ORIENTATION CLASSIFICATION
// ======================================================

#define STANDING_ANGLE_MIN 0.0f
#define STANDING_ANGLE_MAX 30.0f

#define SITTING_ANGLE_MIN 30.0f
#define SITTING_ANGLE_MAX 60.0f

#define LYING_ANGLE_MIN 60.0f
#define LYING_ANGLE_MAX 90.0f

// ======================================================
// FALL TYPE CLASSIFICATION
// ======================================================

#define FALL_FORWARD_ANGLE_MIN 45.0f
#define FALL_BACKWARD_ANGLE_MIN 45.0f
#define FALL_SIDEWAYS_ANGLE_MIN 45.0f

#define ROTATIONAL_FALL_GYRO_MIN 200.0f

// ======================================================
// FILTER CONFIGURATION
// ======================================================

#define MOVING_AVERAGE_WINDOW 5
#define MEDIAN_FILTER_SIZE 3
#define NOISE_THRESHOLD 0.1f

// Complementary filter
#define USE_COMPLEMENTARY_FILTER 1
#define COMPLEMENTARY_ALPHA 0.96f

// ======================================================
// PULSE SENSOR CONFIG
// ======================================================

#define PULSE_THRESHOLD 550          /*!< Ngưỡng để phát hiện nhịp tim - cần hiệu chỉnh thực tế */
#define PULSE_SAMPLE_RATE 100        /*!< Tần số lấy mẫu (Hz) - nên đặt cao để phát hiện chính xác */
#define PULSE_BUFFER_SIZE 50         /*!< Kích thước buffer để lưu trữ giá trị nhịp tim gần đây - dùng để tính BPM trung bình */
#define PULSE_MIN_INTERVAL 300       /*!< Khoảng cách tối thiểu giữa 2 nhịp (ms) - tương ứng 200 BPM */
#define PULSE_MAX_INTERVAL 2000      /*!< Khoảng cách tối đa giữa 2 nhịp (ms) - tương ứng 30 BPM */
#define PULSE_SAMPLES_PER_SECOND 100 /*!< Số mẫu mỗi giây */
#define PULSE_SECONDS_TO_AVG 10      /*!< Số giây để tính BPM trung bình */

// ======================================================
// FEATURE FLAGS
// ======================================================

#define ENABLE_WIFI 1
#define ENABLE_BLE 1
#define ENABLE_SIM 1
#define ENABLE_GPS 1
#define ENABLE_PULSE 1

// ======================================================
// NETWORK CONFIGURATION
// ======================================================

// WiFi
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define WIFI_MAX_RETRY 5

// BLE
#define BLE_DEVICE_NAME "ESP32_Fall_Detector"
#define BLE_MANUFACTURER "YourCompany"

// HTTP API
#define API_ENDPOINT "http://your-server.com/api/fall"
#define API_TIMEOUT_MS 5000

// ======================================================
// SMS CONFIGURATION (SIM800L)
// ======================================================

#define SMS_RECIPIENT "+84123456789"
#define SMS_SEND_TIMEOUT 10000

// ======================================================
// POWER CONFIGURATION
// ======================================================

#define BATTERY_FULL_VOLT 4.2f
#define BATTERY_EMPTY_VOLT 3.3f
#define VOLTAGE_DIVIDER_RATIO 2.0f

#endif