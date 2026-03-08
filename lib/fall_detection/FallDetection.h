/**
 * @file FallDetector.h
 * @brief Thuật toán phát hiện té ngã từ dữ liệu MPU6050
 */

#ifndef FALL_DETECTOR_H
#define FALL_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include <math.h>

// ==================== THUẬT TOÁN ====================
/**
 * @brief Thuật toán phát hiện té ngã dựa trên 3 giai đoạn:
 * 1. Free fall (trạng thái rơi tự do) - gia tốc gần 0g
 * 2. Impact (va chạm) - gia tốc tăng đột ngột > threshold
 * 3. Orientation change (thay đổi tư thế) - góc thay đổi > 60°
 */

// ==================== STATE MACHINE ====================
typedef enum
{
    FALL_STATE_IDLE = 0,   // Bình thường
    FALL_STATE_FREE_FALL,  // Phase 1: Free fall detected
    FALL_STATE_IMPACT,     // Phase 2: Impact detected
    FALL_STATE_POST_FALL,  // Phase 3: Monitor posture
    FALL_STATE_CONFIRMED,  // Fall confirmed
    FALL_STATE_RECOVERY,   // Đứng dậy lại
    FALL_STATE_FALSE_ALARM // False detection
} fall_state_t;

// ==================== TƯ THẾ CƠ THỂ ====================
typedef enum
{
    POSTURE_UNKNOWN = 0,
    POSTURE_STANDING,
    POSTURE_SITTING,
    POSTURE_LYING,
    POSTURE_WALKING,
    POSTURE_RUNNING
} body_posture_t;

// ==================== DỮ LIỆU TÍNH TOÁN ====================
typedef struct
{
    float total_accel;         // Tổng gia tốc (g)
    float vertical_accel;      // Gia tốc thẳng đứng
    float horizontal_accel;    // Gia tốc ngang
    float angle_from_vertical; // Góc so với phương thẳng đứng
    float pitch;               // Góc pitch
    float roll;                // Góc roll
    float jerk;                // Đạo hàm gia tốc (g/s)

    // Dữ liệu từ gyro
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float gyro_magnitude;

    // Góc tích hợp
    float angle_gyro_x;
    float angle_gyro_y;
    float angle_gyro_z;

    // Góc kết hợp (complementary)
    float angle_complementary_x;
    float angle_complementary_y;

    // Phát hiện xoay
    float total_rotation;
    bool is_rotating;

    uint32_t timestamp;
} fall_metrics_t;

// ==================== THIẾU STRUCT NÀY ====================
// Cần thêm struct để lưu trạng thái state machine
typedef struct
{
    fall_state_t current_state;
    body_posture_t current_posture;
    fall_type_t fall_type;

    bool fall_confirmed;
    float fall_confidence;

    uint32_t state_start_time;
    uint32_t free_fall_start_time;
    uint32_t free_fall_end_time;
    uint32_t impact_time;
    uint32_t post_fall_start_time;

    float pre_fall_angle;
    float max_impact_force;
    float max_gyro;
} fall_detector_context_t;

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo bộ phát hiện té ngã 3-phase
 */
void fall_detector_init(void);

/**
 * @brief Cập nhật thuật toán với dữ liệu MPU6050 mới
 * @param mpu_data Dữ liệu từ MPU6050
 */
void fall_detector_update(mpu6050_data_t *mpu_data);

/**
 * @brief Kiểm tra có té ngã không
 * @return true nếu phát hiện té ngã
 */
bool fall_detector_is_fallen(void);

/**
 * @brief Lấy sự kiện té ngã gần nhất
 * @param event Con trỏ lưu event
 * @return true nếu có event
 */
bool fall_detector_get_last_event(fall_event_t *event);

/**
 * @brief Lấy trạng thái hiện tại
 */
fall_state_t fall_detector_get_state(void);

/**
 * @brief Lấy tư thế cơ thể hiện tại
 */
body_posture_t fall_detector_get_posture(void);

/**
 * @brief Reset bộ phát hiện
 */
void fall_detector_reset(void);

/**
 * @brief Hiệu chỉnh
 * @param learning_mode Bật chế độ học
 */
void fall_detector_calibrate(bool learning_mode);

/**
 * @brief Lấy loại té ngã
 */
fall_type_t fall_detector_get_fall_type(void);

#endif // FALL_DETECTOR_H