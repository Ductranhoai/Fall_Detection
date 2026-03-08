/**
 * @file types.h
 * @brief Định nghĩa các kiểu dữ liệu dùng chung trong project
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

// ==================== ENUM ====================

/**
 * @brief Trạng thái hệ thống
 */
typedef enum
{
    SYSTEM_STATE_INIT = 0,      // Đang khởi tạo
    SYSTEM_STATE_NORMAL,        // Hoạt động bình thường
    SYSTEM_STATE_FALL_DETECTED, // Đã phát hiện té ngã
    SYSTEM_STATE_ALERTING,      // Đang cảnh báo
    SYSTEM_STATE_LOW_POWER,     // Pin yếu
    SYSTEM_STATE_ERROR          // Lỗi
} system_state_t;

/**
 * @brief Loại cảm biến
 */
typedef enum
{
    SENSOR_MPU6050 = 0,
    SENSOR_GPS,
    SENSOR_PULSE,
    SENSOR_BATTERY,
    SENSOR_MAX
} sensor_type_t;

/**
 * @brief Trạng thái cảm biến
 */
typedef enum
{
    SENSOR_OK = 0,
    SENSOR_ERROR,
    SENSOR_NOT_FOUND,
    SENSOR_BUSY
} sensor_status_t;

/**
 * @brief Mức độ cảnh báo
 */
typedef enum
{
    ALERT_INFO = 0, // Thông tin
    ALERT_WARNING,  // Cảnh báo
    ALERT_CRITICAL, // Nghiêm trọng
    ALERT_EMERGENCY // Khẩn cấp
} alert_level_t;

/**
 * @brief Loại té ngã
 */
typedef enum
{
    FALL_TYPE_UNKNOWN = 0,    // Không xác định
    FALL_TYPE_FORWARD,        // Ngã về phía trước
    FALL_TYPE_BACKWARD,       // Ngã về phía sau
    FALL_TYPE_SIDEWAYS_LEFT,  // Ngã sang trái
    FALL_TYPE_SIDEWAYS_RIGHT, // Ngã sang phải
    FALL_TYPE_ROTATIONAL,     // Ngã kèm xoay
    FALL_TYPE_SLOW_FALL,      // Ngã chậm (yếu, xỉu)
    FALL_TYPE_FROM_SITTING,   // Ngã từ tư thế ngồi
    FALL_TYPE_FROM_STANDING   // Ngã từ tư thế đứng
} fall_type_t;

/**
 * @brief Chế độ hoạt động
 */
typedef enum
{
    OPERATION_MODE_NORMAL = 0,  // Chế độ bình thường
    OPERATION_MODE_SLEEP,       // Chế độ ngủ
    OPERATION_MODE_DEEP_SLEEP,  // Ngủ sâu
    OPERATION_MODE_CALIBRATION, // Chế độ hiệu chỉnh
    OPERATION_MODE_TEST,        // Chế độ test
    OPERATION_MODE_UPDATE       // Chế độ cập nhật
} operation_mode_t;

/**
 * @brief Nguồn cảnh báo
 */
typedef enum
{
    ALERT_SOURCE_FALL = 0,     // Từ té ngã
    ALERT_SOURCE_BUTTON,       // Từ nút nhấn khẩn cấp
    ALERT_SOURCE_LOW_BATTERY,  // Từ pin yếu
    ALERT_SOURCE_SYSTEM_ERROR, // Từ lỗi hệ thống
    ALERT_SOURCE_REMOTE        // Từ điều khiển từ xa
} alert_source_t;

/**
 * @brief Trạng thái GPS
 */
typedef enum
{
    GPS_STATUS_NO_FIX = 0, // Chưa fix được
    GPS_STATUS_2D_FIX,     // Fix 2D
    GPS_STATUS_3D_FIX,     // Fix 3D
    GPS_STATUS_ERROR       // Lỗi
} gps_status_t;

// ==================== STRUCT ====================

/**
 * @brief Dữ liệu từ MPU6050
 */
typedef struct
{
    float ax;           // Gia tốc trục X (g)
    float ay;           // Gia tốc trục Y (g)
    float az;           // Gia tốc trục Z (g)
    float gx;           // Tốc độ góc trục X (độ/s)
    float gy;           // Tốc độ góc trục Y (độ/s)
    float gz;           // Tốc độ góc trục Z (độ/s)
    float temperature;  // Nhiệt độ (°C)
    uint32_t timestamp; // Thời gian lấy mẫu (ms)
} mpu6050_data_t;

/**
 * @brief Dữ liệu GPS
 */
typedef struct
{
    double latitude;    // Vĩ độ
    double longitude;   // Kinh độ
    float altitude;     // Độ cao (m)
    float speed;        // Tốc độ (km/h)
    float course;       // Hướng di chuyển (độ)
    int satellites;     // Số vệ tinh kết nối
    char time[10];      // Giờ UTC (HH:MM:SS)
    char date[11];      // Ngày (DD/MM/YYYY)
    bool is_fixed;      // Đã fix được tọa độ chưa?
    uint32_t timestamp; // Thời gian lấy mẫu
} gps_data_t;

/**
 * @brief Dữ liệu nhịp tim
 */
typedef struct
{
    int raw_value;           // Giá trị raw từ ADC
    int heart_rate;          // Nhịp tim (BPM)
    bool is_beat;            // Có phát hiện nhịp không?
    float confidence;        // Độ tin cậy (0-1)
    uint32_t last_beat_time; // Thời gian nhịp cuối
} pulse_data_t;

/**
 * @brief Dữ liệu cho thuật toán 3-phase
 */
typedef struct
{
    float total_accel;         // Tổng gia tốc (g)
    float vertical_accel;      // Gia tốc thẳng đứng (g)
    float horizontal_accel;    // Gia tốc ngang (g)
    float angle_from_vertical; // Góc so với phương thẳng đứng (độ)
    float pitch;               // Góc pitch (độ)
    float roll;                // Góc roll (độ)
    float yaw;                 // Góc yaw (độ)
    float jerk;                // Đạo hàm gia tốc (g/s)
    float gyro_magnitude;      // Độ lớn vận tốc góc (độ/s)
    float total_rotation;      // Tổng góc quay (độ)
    bool is_rotating;          // Đang xoay không?
    uint32_t timestamp;        // Thời gian (ms)
} fall_metrics_t;

/**
 * @brief Dữ liệu phát hiện té ngã
 */
typedef struct
{
    bool fall_detected;      // Có té ngã không?
    float impact_force;      // Lực tác động (g)
    float angle_change;      // Thay đổi góc (độ)
    float max_gyro;          // Tốc độ góc lớn nhất
    fall_type_t fall_type;   // Loại té ngã
    uint32_t detection_time; // Thời gian phát hiện
    gps_data_t location;     // Vị trí khi té
    pulse_data_t pulse;      // Nhịp tim khi té
    uint8_t confidence;      // Độ tin cậy (0-100%)
} fall_event_t;

/**
 * @brief Dữ liệu hiệu chỉnh (calibration)
 */
typedef struct
{
    float accel_offset[3];   // Offset gia tốc
    float gyro_offset[3];    // Offset con quay
    float reference_angle_x; // Góc tham chiếu trục X
    float reference_angle_y; // Góc tham chiếu trục Y
    float reference_accel;   // Gia tốc tham chiếu
    bool calibrated;         // Đã hiệu chỉnh chưa?
} calibration_data_t;

/**
 * @brief Cấu hình thuật toán phát hiện
 */
typedef struct
{
    float free_fall_threshold;     // Ngưỡng rơi tự do (g)
    float impact_threshold;        // Ngưỡng va chạm (g)
    float orientation_threshold;   // Ngưỡng thay đổi góc (độ)
    uint32_t free_fall_time_min;   // Thời gian rơi tối thiểu (ms)
    uint32_t free_fall_time_max;   // Thời gian rơi tối đa (ms)
    uint32_t impact_window;        // Cửa sổ chờ va chạm (ms)
    uint32_t post_fall_time;       // Thời gian theo dõi sau té (ms)
    bool use_complementary_filter; // Dùng complementary filter?
    float complementary_alpha;     // Hệ số complementary filter
} algorithm_config_t;

/**
 * @brief Cấu trúc cảnh báo
 */
typedef struct
{
    alert_level_t level;    // Mức độ cảnh báo
    alert_source_t source;  // Nguồn cảnh báo
    char message[256];      // Nội dung cảnh báo
    fall_event_t fall_data; // Dữ liệu té ngã
    bool sent_wifi;         // Đã gửi qua WiFi?
    bool sent_ble;          // Đã gửi qua BLE?
    bool sent_sms;          // Đã gửi SMS?
    uint32_t sent_time;     // Thời gian gửi
    uint8_t retry_count;    // Số lần thử lại
} alert_t;

/**
 * @brief Trạng thái pin
 */
typedef struct
{
    float voltage;         // Điện áp (V)
    float percentage;      // Phần trăm pin (0-100)
    bool is_charging;      // Đang sạc không?
    bool low_battery;      // Pin yếu?
    bool critical_battery; // Pin cực yếu?
} battery_status_t;

/**
 * @brief Thông tin thiết bị
 */
typedef struct
{
    char device_id[32];              // ID thiết bị
    char firmware_version[16];       // Phiên bản firmware
    uint32_t uptime;                 // Thời gian hoạt động (s)
    uint32_t free_heap;              // Bộ nhớ heap còn trống
    int8_t wifi_rssi;                // Cường độ WiFi (dBm)
    bool wifi_connected;             // Đã kết nối WiFi?
    bool ble_connected;              // Đã kết nối BLE?
    bool sim_ready;                  // SIM sẵn sàng?
    operation_mode_t operation_mode; // Chế độ hoạt động
} device_info_t;

/**
 * @brief Thống kê phát hiện
 */
typedef struct
{
    uint32_t total_falls;       // Tổng số lần té ngã
    uint32_t false_alarms;      // Số lần báo động giả
    uint32_t successful_alerts; // Số lần cảnh báo thành công
    uint32_t last_fall_time;    // Thời gian té gần nhất
    float avg_impact_force;     // Lực va chạm trung bình
    uint32_t start_time;        // Thời gian bắt đầu ghi nhận
    uint32_t total_uptime;      // Tổng thời gian hoạt động
} statistics_t;

/**
 * @brief Gói dữ liệu gửi lên server
 */
typedef struct
{
    char device_id[32];        // ID thiết bị
    uint32_t timestamp;        // Thời gian gửi
    fall_event_t fall_data;    // Dữ liệu té ngã
    battery_status_t battery;  // Trạng thái pin
    gps_data_t location;       // Vị trí
    device_info_t device_info; // Thông tin thiết bị
    statistics_t statistics;   // Thống kê
} api_packet_t;

/**
 * @brief Cấu hình mạng
 */
typedef struct
{
    char wifi_ssid[32];     // Tên WiFi
    char wifi_password[64]; // Mật khẩu WiFi
    char server_url[128];   // URL server
    uint16_t server_port;   // Cổng server
    bool use_ssl;           // Dùng SSL?
    uint32_t timeout_ms;    // Timeout kết nối
    uint8_t max_retries;    // Số lần thử lại tối đa
} network_config_t;

/**
 * @brief Dữ liệu debug
 */
typedef struct
{
    float cpu_usage;           // CPU usage (%)
    float temperature;         // Nhiệt độ MCU
    uint32_t stack_high_water; // Stack còn lại
    uint32_t i2c_errors;       // Số lỗi I2C
    uint32_t uart_errors;      // Số lỗi UART
    char last_error[128];      // Lỗi gần nhất
} debug_info_t;

#endif // TYPES_H