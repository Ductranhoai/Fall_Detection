/**
 * @file FallDetector.c
 * @brief Implementation thuật toán phát hiện té ngã 3-phase
 */

#include "FallDetection.h"
#include "config.h"
#include "types.h"
#include <math.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "FALL_DETECTOR";

// ==================== BIẾN STATIC ====================
static fall_detector_context_t ctx;
static fall_event_t last_event;
static fall_metrics_t metrics_history[50];
static int history_index = 0;

// Bộ lọc
static float accel_buffer[MOVING_AVERAGE_WINDOW];
static int accel_buffer_index = 0;

// Hieu chinh
static bool calibration_mode = false;
static int calibration_samples = 0;
static float accel_offset[3] = {0};
static float gyro_offset[3] = {0};

// ==================== STATIC FUNCTION PROTOTYPES ====================
static void handle_calibration(mpu6050_data_t *mpu_data);
static void update_state_machine(fall_metrics_t *metrics, uint32_t current_time);
static void confirm_fall(fall_metrics_t *metrics, uint32_t current_time);
static void handle_completion_state(uint32_t current_time);

// ==================== HÀM STATIC ====================
/**
 * @brief Tính gia tốc tổng hợp
 * |a| = sqrt(ax^2 + ay^2 + az^2)
 * sqrtf là hàm căn bậc hai cho float, nhanh hơn sqrt() cho double
 */
static float calculate_total_accelration(float ax, float ay, float az)
{
    return sqrtf(ax * ax + ay * ay + az * az);
}

/**
 * @brief Tính góc so với phương thẳng đứng
 * Góc giữa vector gia tốc và trục Z (phương thẳng đứng):
 * angle = acos(az / |a|)
 */
static float calculate_vertical_angle(float ax, float ay, float az)
{
    float total_accel = calculate_total_accelration(ax, ay, az); // |a|
    if (total_accel < 0.01f)
        return 0.0f; // Tránh chia cho 0, nếu gia tốc quá nhỏ coi như không có góc

    float cos_theta = az / total_accel; // cos(θ) = az / |a|
    if (cos_theta > 1.0f)
        cos_theta = 1.0f; // Giới hạn giá trị cos(θ) trong khoảng [-1, 1]
    else if (cos_theta < -1.0f)
        cos_theta = -1.0f;

    return acosf(cos_theta) * 180.0f / 3.14159f; // Convert sang độ
}

/**
 * @brief Tính góc pitch từ gia tốc (góc nghiêng về phía trước/sau)
 * Công thức: pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180/π
 * atan2 : hàm arctangent 2 tham số - trả về góc giữa trục x dương và điểm (x, y) trên mặt phẳng tọa độ.
 */
static float calculate_pitch(float ax, float ay, float az)
{
    return atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / 3.14159f;
}

/**
 * @brief Tính góc roll từ gia tốc
 * Công thức: roll = atan2(ay, az) * 180/π
 */
static float calculate_roll(float ax, float ay, float az)
{
    return atan2f(ay, az) * 180.0f / 3.14159f;
}

/**
 * @brief Tính jerk (đạo hàm gia tốc)
 * j=Δtacurrent​−aprevious​/Δt
 */
static float calculate_jerk(float current_accel, float previous_accel, uint32_t dt_ms)
{
    if (dt_ms == 0)
        return 0.0f;                                                    // Tránh chia cho 0
    return (current_accel - previous_accel) / ((float)dt_ms / 1000.0f); // Đổi ms sang giây
}

/**
 * @brief Lọc trung bình động
 * y[n]= x[n]+x[n−1]+x[n−2]+...+x[n−N+1] ​/ N
 *
 * x[n] = dữ liệu sensor tại thời điểm n
 * N = số mẫu dùng để trung bình
 * y[n] = giá trị đã lọc tại thời điểm n
 */
static float moving_average_filter(float new_value)
{
    static float buffer_sum = 0;
    static bool buffer_filled = false;
    static int current_count = 0;

    // Khởi tạo
    if (!buffer_filled)
    {
        for (int i = 0; i < MOVING_AVERAGE_WINDOW; i++)
        {
            accel_buffer[i] = new_value;
            buffer_sum += new_value;
        }
        buffer_filled = true;
        accel_buffer_index = 0;
        return new_value;
    }

    // Cập nhật tổng: trừ giá trị cũ, thêm giá trị mới
    buffer_sum -= accel_buffer[accel_buffer_index];
    buffer_sum += new_value;

    // Lưu giá trị mới vào buffer
    accel_buffer[accel_buffer_index] = new_value;
    accel_buffer_index = (accel_buffer_index + 1) % MOVING_AVERAGE_WINDOW;

    return buffer_sum / MOVING_AVERAGE_WINDOW;
}

/**
 * @brief Complementary filter kết hợp accel và gyro
 * angle = alpha * (angle + gyro*dt) + (1-alpha) * accel_angle
 */
static void complementary_filter(fall_metrics_t *metrics, mpu6050_data_t *mpu_data, uint32_t dt_ms)
{
    static float prev_pitch = 0, prev_roll = 0;
    static uint32_t last_time = 0;

    // Thêm kiểm tra dt hợp lệ
    if (dt_ms == 0 || dt_ms > 100)
    { // Bỏ qua nếu dt quá lớn (>100ms)
        metrics->angle_complementary_x = metrics->roll;
        metrics->angle_complementary_y = metrics->pitch;
        return;
    }

    if (last_time == 0)
    {
        prev_pitch = metrics->pitch;
        prev_roll = metrics->roll;
        last_time = mpu_data->timestamp;

        metrics->angle_complementary_x = metrics->roll;
        metrics->angle_complementary_y = metrics->pitch;
        return;
    }

    float dt = dt_ms / 1000.0f;

    // Tích phân từ gyro với drift compensation
    float gyro_pitch = prev_pitch + mpu_data->gy * dt;
    float gyro_roll = prev_roll + mpu_data->gx * dt;

    // Complementary filter với hệ số thích ứng
    float alpha = COMPLEMENTARY_ALPHA;

    // Nếu có chuyển động mạnh, tin tưởng gyro hơn
    if (metrics->gyro_magnitude > GYRO_THRESHOLD)
    {
        alpha = 0.98f; // Tin gyro nhiều hơn khi có chuyển động
    }

    metrics->angle_complementary_x = alpha * gyro_roll +
                                     (1 - alpha) * metrics->roll;
    metrics->angle_complementary_y = alpha * gyro_pitch +
                                     (1 - alpha) * metrics->pitch;

    prev_pitch = metrics->angle_complementary_y;
    prev_roll = metrics->angle_complementary_x;
    last_time = mpu_data->timestamp;
}

// ==================== PHÁT HIỆN CÁC PHASE ====================

/**
 * @brief Phase 1: Free Fall Detection
 */
static bool check_free_fall_phase(fall_metrics_t *metrics)
{
    // Điều kiện 1: Gia tốc < ngưỡng
    if (metrics->total_accel > FREE_FALL_THRESHOLD)
    {
        return false;
    }

    // Điều kiện 2: Jerk nhỏ (không có chuyển động đột ngột)
    if (fabsf(metrics->jerk) > FREE_FALL_JERK_THRESHOLD)
    {
        return false;
    }

    // Điều kiện 3: Kiểm tra lịch sử
    int count = 0;
    for (int i = 0; i < 5; i++)
    {
        int idx = (history_index - i - 1 + 50) % 50;
        if (metrics_history[idx].total_accel < FREE_FALL_THRESHOLD)
        {
            count++;
        }
    }
    return count >= 3;
}

/**
 * @brief Phase 2: Impact Detection
 */
static bool check_impact_phase(fall_metrics_t *metrics)
{
    // Điều kiện 1: Gia tốc > ngưỡng
    if (metrics->total_accel < IMPACT_THRESHOLD)
    {
        return false;
    }

    // Điều kiện 2: Jerk lớn (va chạm đột ngột)
    if (fabsf(metrics->jerk) < IMPACT_JERK_THRESHOLD)
    {
        return false;
    }

    // Điều kiện 3: Thành phần thẳng đứng chiếm ưu thế
    float v_over_h = metrics->vertical_accel / (metrics->horizontal_accel + NOISE_THRESHOLD);
    if (v_over_h < VERTICAL_IMPACT_RATIO)
    {
        return false; // Va chạm ngang (ít nguy hiểm hơn)
    }

    // Lưu lực va chạm lớn nhất
    if (metrics->total_accel > ctx.max_impact_force)
    {
        ctx.max_impact_force = metrics->total_accel;
    }

    return true;
}

/**
 * @brief Phase 3: Post Fall Detection
 */
static bool check_post_fall_phase(fall_metrics_t *metrics)
{
    // Điều kiện 1: Góc thay đổi lớn
    float angle_change = fabsf(metrics->angle_from_vertical - ctx.pre_fall_angle);
    if (angle_change < ORIENTATION_THRESHOLD)
    {
        return false;
    }

    // Điều kiện 2: Đang ở tư thế nằm
    if (metrics->angle_from_vertical < LYING_ANGLE_MIN)
    {
        return false;
    }

    // Điều kiện 3: Bất động (gia tốc gần 1g)
    if (fabsf(metrics->total_accel - 1.0f) > (NOISE_THRESHOLD * 2.0f))
    {
        return false;
    }

    // Điều kiện 4: Jerk nhỏ (không cử động)
    if (fabsf(metrics->jerk) > (GYRO_STABLE_THRESHOLD / 20.0f))
    {
        return false;
    }

    return true;
}

// ==================== PHÂN LOẠI LOẠI TÉ NGÃ ====================

/**
 * @brief Xác định loại té ngã dựa trên dữ liệu
 */
static fall_type_t classify_fall_type(fall_metrics_t *metrics, float pre_fall_angle)
{
    fall_type_t type = FALL_TYPE_UNKNOWN;

    // xac dinh dua vao goc thay doi va dau cua gia toc
    float angle_change = fabsf(metrics->angle_from_vertical - pre_fall_angle);

    // Kiem tra xoay (rotational fall)
    if (metrics->total_rotation > ROTATIONAL_FALL_GYRO_MIN)
    {
        return FALL_TYPE_ROTATIONAL;
    }

    // Phan loai dua vao huong thay doi goc
    if (angle_change > 10.0f) // Tăng góc -> ngả về phía sau
    {
        type = FALL_TYPE_BACKWARD;
    }
    else if (angle_change < -10.0f) // Giảm góc -> ngả về phía trước
    {
        type = FALL_TYPE_FORWARD;
    }
    else // Thay đổi nhỏ -> có thể là ngã ngang
    {
        if (fabsf(metrics->roll) > 45.0f)
        {
            type = (metrics->roll > 0) ? FALL_TYPE_SIDEWAYS_RIGHT : FALL_TYPE_SIDEWAYS_LEFT;
        }
    }

    // Kiểm tra tư thế ban đầu
    if (type != FALL_TYPE_UNKNOWN && ctx.current_posture == POSTURE_SITTING)
    {
        type = FALL_TYPE_FROM_SITTING;
    }

    return type;
}

// ==================== PHÁT HIỆN TƯ THẾ ====================

/**
 * @brief Xác định tư thế hiện tại
 */
static body_posture_t detect_posture(fall_metrics_t *metrics)
{
    if (metrics->total_accel < 0.8f || metrics->total_accel > 1.2f)
    {
        return POSTURE_UNKNOWN;
    }

    float angle = metrics->angle_from_vertical;

    if (angle <= STANDING_ANGLE_MAX)
    {
        return POSTURE_STANDING;
    }
    else if (angle <= SITTING_ANGLE_MAX)
    {
        return POSTURE_SITTING;
    }
    else
    {
        return POSTURE_LYING;
    }
}

/**
 * @brief Tính toán các metrics từ dữ liệu MPU6050
 */
static void calculate_metrics(mpu6050_data_t *mpu_data, fall_metrics_t *metrics)
{
    static mpu6050_data_t prev_data = {0};
    static uint32_t prev_time = 0;
    static float integrated_gyro_x = 0, integrated_gyro_y = 0, integrated_gyro_z = 0;
    static float total_rotation = 0;
    static uint32_t rotation_start_time = 0;
    static float prev_total_accel = 1.0f; // Giá trị khởi tạo hợp lý

    // Reset metrics
    memset(metrics, 0, sizeof(fall_metrics_t));

    // Áp dụng offset hiệu chỉnh
    float ax = mpu_data->ax - accel_offset[0];
    float ay = mpu_data->ay - accel_offset[1];
    float az = mpu_data->az - accel_offset[2];
    float gx = mpu_data->gx - gyro_offset[0];
    float gy = mpu_data->gy - gyro_offset[1];
    float gz = mpu_data->gz - gyro_offset[2];

    // Lọc nhiễu cho accel
    ax = moving_average_filter(ax); // Dùng hàm đã sửa ở trên

    // Tính các giá trị từ accel
    metrics->total_accel = calculate_total_accelration(ax, ay, az);
    metrics->vertical_accel = fabsf(az);
    metrics->horizontal_accel = sqrtf(ax * ax + ay * ay);
    metrics->angle_from_vertical = calculate_vertical_angle(ax, ay, az);
    metrics->pitch = calculate_pitch(ax, ay, az);
    metrics->roll = calculate_roll(ax, ay, az);

    // Dữ liệu gyro
    metrics->gyro_x = gx;
    metrics->gyro_y = gy;
    metrics->gyro_z = gz;
    metrics->gyro_magnitude = sqrtf(gx * gx + gy * gy + gz * gz);

    // Tính các giá trị cần thời gian
    if (prev_time != 0 && mpu_data->timestamp > prev_time)
    {
        uint32_t dt_ms = mpu_data->timestamp - prev_time;
        float dt = (dt_ms > 0) ? dt_ms / 1000.0f : 0.01f; // Tránh chia 0

        // Tính Jerk
        metrics->jerk = (metrics->total_accel - prev_total_accel) / dt;

        // Tích phân góc từ gyro
        integrated_gyro_x += gx * dt;
        integrated_gyro_y += gy * dt;
        integrated_gyro_z += gz * dt;

        // Giới hạn góc
        metrics->angle_gyro_x = fmodf(integrated_gyro_x, 360.0f);
        metrics->angle_gyro_y = fmodf(integrated_gyro_y, 360.0f);
        metrics->angle_gyro_z = fmodf(integrated_gyro_z, 360.0f);

        // Tính tổng góc xoay
        if (metrics->gyro_magnitude > GYRO_THRESHOLD)
        {
            if (!metrics->is_rotating)
            {
                rotation_start_time = mpu_data->timestamp;
                total_rotation = 0;
            }
            total_rotation += metrics->gyro_magnitude * dt;
            metrics->is_rotating = true;

            // Cập nhật max_gyro
            if (metrics->gyro_magnitude > ctx.max_gyro)
            {
                ctx.max_gyro = metrics->gyro_magnitude;
            }
        }
        else
        {
            metrics->is_rotating = false;
            if (mpu_data->timestamp - rotation_start_time > 1000)
            {
                total_rotation = 0;
            }
        }
        metrics->total_rotation = total_rotation;
    }
    else
    {
        // Lần đầu tiên
        metrics->jerk = 0;
        metrics->angle_gyro_x = 0;
        metrics->angle_gyro_y = 0;
        metrics->angle_gyro_z = 0;
        metrics->total_rotation = 0;
        metrics->is_rotating = false;

        integrated_gyro_x = 0;
        integrated_gyro_y = 0;
        integrated_gyro_z = 0;
        total_rotation = 0;
        rotation_start_time = mpu_data->timestamp;
    }

    // Complementary filter
    if (USE_COMPLEMENTARY_FILTER && prev_time != 0)
    {
        complementary_filter(metrics, mpu_data, mpu_data->timestamp - prev_time);
    }
    else
    {
        metrics->angle_complementary_x = metrics->roll;
        metrics->angle_complementary_y = metrics->pitch;
    }

    metrics->timestamp = mpu_data->timestamp;

    // Lưu giá trị cho lần sau
    prev_data = *mpu_data;
    prev_total_accel = metrics->total_accel;
    prev_time = mpu_data->timestamp;
}

// ==================== API PUBLIC ====================

void fall_detector_init(void)
{
    ESP_LOGI(TAG, "Initializing 3-phase Fall Detector");

    // Reset context
    memset(&ctx, 0, sizeof(fall_detector_context_t));
    ctx.current_state = FALL_STATE_IDLE;
    ctx.current_posture = POSTURE_UNKNOWN;
    ctx.fall_confirmed = false;
    ctx.fall_confidence = 0.0f;

    // Reset history
    memset(metrics_history, 0, sizeof(metrics_history));
    history_index = 0;

    // Reset filters
    memset(accel_buffer, 0, sizeof(accel_buffer));
    accel_buffer_index = 0;

    ESP_LOGI(TAG, "3-phase Fall Detector ready");
}

void fall_detector_update(mpu6050_data_t *mpu_data)
{
    if (mpu_data == NULL)
        return;

    uint32_t current_time = mpu_data->timestamp;

    // ===== 1. TÍNH TOÁN METRICS - DÙNG HÀM CHUẨN =====
    fall_metrics_t metrics;
    calculate_metrics(mpu_data, &metrics);

    // Lưu vào history
    metrics_history[history_index] = metrics;
    history_index = (history_index + 1) % 50;

    // ===== 2. CHẾ ĐỘ HIỆU CHỈNH =====
    if (calibration_mode)
    {
        handle_calibration(mpu_data); // Tách thành hàm riêng
        return;
    }

    // ===== 3. CẬP NHẬT TƯ THẾ =====
    ctx.current_posture = detect_posture(&metrics);

    // ===== 4. STATE MACHINE =====
    update_state_machine(&metrics, current_time);
}

static void update_state_machine(fall_metrics_t *metrics, uint32_t current_time)
{
    switch (ctx.current_state)
    {
    case FALL_STATE_IDLE:
        if (check_free_fall_phase(metrics))
        {
            ctx.current_state = FALL_STATE_FREE_FALL;
            ctx.free_fall_start_time = current_time;
            ctx.pre_fall_angle = metrics->angle_from_vertical;
            ESP_LOGD(TAG, "Phase 1: Free Fall detected");
        }
        break;

    case FALL_STATE_FREE_FALL:
        if (current_time - ctx.free_fall_start_time > FREE_FALL_TIME_MAX)
        {
            ctx.current_state = FALL_STATE_IDLE;
        }
        else if (current_time - ctx.free_fall_start_time >= FREE_FALL_TIME_MIN)
        {
            ctx.free_fall_end_time = current_time;
            ctx.current_state = FALL_STATE_IMPACT;
            ESP_LOGD(TAG, "Phase 2: Waiting for impact");
        }
        break;

    case FALL_STATE_IMPACT:
        if (check_impact_phase(metrics))
        {
            ctx.impact_time = current_time;
            ctx.current_state = FALL_STATE_POST_FALL;
            ctx.post_fall_start_time = current_time;
            ESP_LOGD(TAG, "Phase 2: Impact detected!");
        }
        else if (current_time - ctx.free_fall_end_time > IMPACT_WINDOW)
        {
            ctx.current_state = FALL_STATE_IDLE;
        }
        break;

    case FALL_STATE_POST_FALL:
        if (check_post_fall_phase(metrics))
        {
            confirm_fall(metrics, current_time); // Tách thành hàm riêng
        }
        else if (current_time - ctx.post_fall_start_time > POST_FALL_TIME)
        {
            ctx.current_state = FALL_STATE_FALSE_ALARM;
        }
        break;

    case FALL_STATE_CONFIRMED:
    case FALL_STATE_FALSE_ALARM:
        handle_completion_state(current_time);
        break;
    }
}

static void handle_calibration(mpu6050_data_t *mpu_data)
{
    if (calibration_samples < 100)
    {
        // Cộng dồn mẫu
        accel_offset[0] += mpu_data->ax;
        accel_offset[1] += mpu_data->ay;
        accel_offset[2] += mpu_data->az;
        gyro_offset[0] += mpu_data->gx;
        gyro_offset[1] += mpu_data->gy;
        gyro_offset[2] += mpu_data->gz;
        calibration_samples++;
    }
    else if (calibration_samples == 100)
    {
        // Tính trung bình
        for (int i = 0; i < 3; i++)
        {
            accel_offset[i] /= 100.0f;
            gyro_offset[i] /= 100.0f;

            // Gyro lý tưởng là 0 khi đứng yên
            // Nên offset chính là giá trị đọc được
        }
        calibration_samples++;
        ESP_LOGI(TAG, "Calibration complete:");
        ESP_LOGI(TAG, "Accel offsets: %.2f, %.2f, %.2f",
                 accel_offset[0], accel_offset[1], accel_offset[2]);
        ESP_LOGI(TAG, "Gyro offsets: %.2f, %.2f, %.2f",
                 gyro_offset[0], gyro_offset[1], gyro_offset[2]);
    }
}

static void confirm_fall(fall_metrics_t *metrics, uint32_t current_time)
{
    ctx.current_state = FALL_STATE_CONFIRMED;
    ctx.fall_confirmed = true;

    // Tạo event
    last_event.fall_detected = true;
    last_event.impact_force = ctx.max_impact_force;
    last_event.angle_change = fabsf(metrics->angle_from_vertical - ctx.pre_fall_angle);
    last_event.max_gyro = ctx.max_gyro;
    last_event.fall_type = classify_fall_type(metrics, ctx.pre_fall_angle);
    last_event.detection_time = current_time;
    last_event.confidence = 95;

    ESP_LOGW(TAG, "=== FALL CONFIRMED! Type: %d ===", last_event.fall_type);
}

static void handle_completion_state(uint32_t current_time)
{
    // Tự động reset sau 5 giây
    if (current_time - ctx.post_fall_start_time > 5000)
    {
        ctx.current_state = FALL_STATE_IDLE;
        ctx.fall_confirmed = false;
    }
}

bool fall_detector_is_fallen(void)
{
    if (ctx.fall_confirmed)
    {
        ctx.fall_confirmed = false;
        return true;
    }
    return false;
}

bool fall_detector_get_last_event(fall_event_t *event)
{
    if (event == NULL)
        return false;

    memcpy(event, &last_event, sizeof(fall_event_t));
    return last_event.fall_detected;
}

fall_state_t fall_detector_get_state(void)
{
    return ctx.current_state;
}

body_posture_t fall_detector_get_posture(void)
{
    return ctx.current_posture;
}

void fall_detector_reset(void)
{
    ctx.current_state = FALL_STATE_IDLE;
    ctx.fall_confirmed = false;
    ctx.max_impact_force = 0;
    ctx.max_gyro = 0;
    ESP_LOGI(TAG, "Fall detector reset");
}

void fall_detector_calibrate(bool learning_mode)
{
    calibration_mode = learning_mode;
    calibration_samples = 0;

    if (learning_mode)
    {
        memset(accel_offset, 0, sizeof(accel_offset));
        memset(gyro_offset, 0, sizeof(gyro_offset));
        ESP_LOGI(TAG, "Calibration started - keep device still");
    }
    else
    {
        ESP_LOGI(TAG, "Calibration finished");
    }
}

fall_type_t fall_detector_get_fall_type(void)
{
    return last_event.fall_type;
}