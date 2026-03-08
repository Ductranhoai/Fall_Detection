/**
 * @file PLUSEDriver.h
 * @brief Driver cho Pulse Sensor (cảm biến nhịp tim quang học)
 *
 * Nguyên lý: Đo sự thay đổi cường độ ánh sáng phản xạ từ mạch máu
 * Sử dụng ADC để đọc giá trị analog, phát hiện đỉnh nhịp tim
 */

#ifndef PULSE_DRIVER_H
#define PULSE_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "config.h"

// ==================== CẤU TRÚC DỮ LIỆU ====================

/**
 * @brief Bộ lọc trung bình động cho pulse
 */
typedef struct
{
    int buffer[PULSE_BUFFER_SIZE]; /*!< Buffer lưu giá trị */
    int index;                     /*!< Vị trí hiện tại */
    int sum;                       /*!< Tổng các giá trị */
    int count;                     /*!< Số phần tử trong buffer */
} moving_average_filter_t;

/**
 * @brief Bộ phát hiện nhịp tim
 */
typedef struct
{
    int signal;      /*!< Giá trị tín hiệu hiện tại (đã lọc) */
    int prev_signal; /*!< Giá trị trước đó */
    int threshold;   /*!< Ngưỡng phát hiện */

    bool beat_detected;      /*!< Có phát hiện nhịp không */
    uint32_t last_beat_time; /*!< Thời điểm nhịp cuối (ms) */
    uint32_t beat_interval;  /*!< Khoảng cách giữa 2 nhịp (ms) */

    int bpm;            /*!< Nhịp tim hiện tại (BPM) */
    int bpm_buffer[10]; /*!< Buffer lưu BPM gần nhất */
    int bpm_index;      /*!< Vị trí trong buffer BPM */
    int bpm_sum;        /*!< Tổng BPM để tính trung bình */

    int samples_since_last_beat; /*!< Số mẫu từ nhịp cuối */
    bool rising;                 /*!< Tín hiệu đang tăng? */
} pulse_detector_t;

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo Pulse Sensor
 * @return true nếu thành công
 */
bool pulse_init(void);

/**
 * @brief Đọc giá trị từ Pulse Sensor
 * @param data Con trỏ tới struct để lưu dữ liệu
 * @return true nếu đọc thành công
 */
bool pulse_read(pulse_data_t *data);

/**
 * @brief Cập nhật thuật toán phát hiện nhịp tim
 * Hàm này cần được gọi đều đặn với tần số PULSE_SAMPLE_RATE
 */
void pulse_update(void);

/**
 * @brief Lấy nhịp tim hiện tại (BPM)
 * @return Nhịp tim (0 nếu chưa detect được)
 */
int pulse_get_heart_rate(void);

/**
 * @brief Kiểm tra có phát hiện nhịp mới không
 * @return true nếu vừa có nhịp
 */
bool pulse_is_beat(void);

/**
 * @brief Hiệu chỉnh ngưỡng phát hiện tự động
 * @param raw_value Giá trị raw từ ADC
 */
void pulse_auto_threshold(int raw_value);

/**
 * @brief Reset bộ phát hiện
 */
void pulse_reset(void);

#endif // PULSE_DRIVER_H