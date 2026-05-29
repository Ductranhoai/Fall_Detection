#include "mpu_manager.h"
#include "fall_detection.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "MPU_MANAGER";

/**
 * @brief Biến tĩnh quản lý trạng thái của MPU Manager
 * ====================================================
 * s_mpu_config:         Cấu hình hiện tại của MPU Manager
 * s_initialized:        Cờ kiểm tra đã khởi tạo manager thành công
 * s_read_task_handle:   Handle của task đọc dữ liệu liên tục
 * s_data_callback:      Hàm callback được gọi khi có dữ liệu mới
 * s_sample_interval_ms: Chu kỳ đọc dữ liệu (millisecond)
 */
static mpu_config_t s_mpu_config;
static bool s_initialized = false;
static TaskHandle_t s_read_task_handle = NULL;
static mpu_data_callback_t s_data_callback = NULL;
static uint32_t s_sample_interval_ms = 0;

/* Khai báo trước các hàm nội bộ */
static void continuous_read_task(void *arg);

/**
 * @brief Hàm callback mặc định khi không có callback được đăng ký
 * ================================================================
 * @param data  Dữ liệu MPU6050 vừa đọc được
 * 
 * Chức năng:
 * 1. Xử lý phát hiện té ngã (fall detection)
 * 2. Log thông tin khi phát hiện té ngã (cảnh báo)
 * 3. Log trạng thái bình thường mỗi 2 giây
 * 
 * Chi tiết xử lý:
 * - Gọi fall_detection_process() để phân tích dữ liệu
 * - Lấy kết quả phát hiện té ngã
 * - Nếu phát hiện té ngã: Log chi tiết (lý do, lực tác động, góc nghiêng)
 * - Nếu bình thường: Log định kỳ state, góc pitch/roll, gia tốc Z
 */
static void default_callback(mpu6050_data_t *data)
{
    static uint32_t last_log = 0;
    int64_t now = esp_timer_get_time() / 1000;  // Lấy timestamp hiện tại (ms)
    
    /* Xử lý thuật toán phát hiện té ngã */
    fall_detection_process(data);
    fall_result_t result = fall_detection_get_result();
    
    /* Log ngay lập tức khi phát hiện té ngã (cấp độ WARNING) */
    if (result.fall_detected) {
        ESP_LOGW(TAG, "  FALL DETECTED! ");
        ESP_LOGW(TAG, "  Reason: %s", result.detection_reason);
        ESP_LOGW(TAG, "  Max impact: %.2fg", result.max_accel);
        ESP_LOGW(TAG, "  Final tilt: %.1f°", result.final_tilt);
    }
    
    /* Log trạng thái bình thường mỗi 2 giây để giám sát */
    if (now - last_log > 2000) {
        ESP_LOGI(TAG, "Status - State: %s, Pitch: %.1f°, Roll: %.1f°, Z: %.2fg",
                 fall_state_to_string(result.state),
                 data->pitch, data->roll, data->accel_z);
        last_log = now;
    }
}

/**
 * @brief Task đọc dữ liệu MPU6050 liên tục (chạy trên FreeRTOS)
 * ==============================================================
 * @param arg  Tham số không sử dụng (NULL)
 * 
 * Hoạt động:
 * - Vòng lặp vô tận chạy trong task riêng
 * - Đọc dữ liệu từ MPU6050 qua I2C
 * - Tính toán góc pitch/roll từ dữ liệu accelerometer
 * - Gọi callback (nếu được đăng ký) để xử lý dữ liệu
 * - Delay chính xác để đạt tần số lấy mẫu mong muốn
 * 
 * Ưu điểm:
 * - Không block main task
 * - Độ ưu tiên 5 (cao) đảm bảo đọc dữ liệu đúng chu kỳ
 * - Sử dụng vTaskDelayUntil để delay chính xác
 */
static void continuous_read_task(void *arg)
{
    mpu6050_data_t data;
    TickType_t last_wake_time = xTaskGetTickCount();

    printf(">>> MPU Continuous Read Task Started <<<\n");

    while (1)
    {
        /* Đọc dữ liệu từ MPU6050 */
        if (mpu6050_read(&data) == ESP_OK)
        {
            /* Tính góc pitch/roll từ accelerometer */
            mpu6050_calc_angles(&data);

            /* Gọi callback nếu có (callback có thể là default hoặc user-defined) */
            if (s_data_callback)
            {
                s_data_callback(&data);
            }
        }

        /* Delay chính xác đến chu kỳ tiếp theo (không bị trôi tích lũy) */
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(s_sample_interval_ms));
    }
}

/**
 * @brief Khởi tạo MPU Manager với cấu hình tùy chỉnh
 * ===================================================
 * @param config  Con trỏ đến cấu hình MPU
 * @return esp_err_t ESP_OK nếu thành công
 * 
 * Quy trình khởi tạo:
 * 1. Kiểm tra chưa được khởi tạo
 * 2. Copy cấu hình vào biến tĩnh
 * 3. Chuyển đổi cấu hình sang định dạng của driver MPU6050
 * 4. Khởi tạo driver MPU6050
 * 5. Calibrate cảm biến nếu được yêu cầu
 * 6. Đánh dấu đã khởi tạo
 * 7. Khởi tạo module phát hiện té ngã với cấu hình mặc định
 * 
 * Lưu ý: Calibration yêu cầu cảm biến phải đứng yên hoàn toàn
 */
esp_err_t mpu_manager_init(mpu_config_t *config)
{
    if (s_initialized)
    {
        ESP_LOGW(TAG, "MPU already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Lưu cấu hình */
    memcpy(&s_mpu_config, config, sizeof(mpu_config_t));

    /* Chuyển đổi cấu hình sang định dạng của MPU6050 driver */
    mpu6050_config_t mpu_config = mpu6050_get_default_config();
    mpu_config.i2c_port = s_mpu_config.i2c_port;
    mpu_config.i2c_freq_hz = s_mpu_config.i2c_freq;
    mpu_config.sda_pin = s_mpu_config.sda_pin;
    mpu_config.scl_pin = s_mpu_config.scl_pin;
    mpu_config.accel_range = s_mpu_config.accel_range;
    mpu_config.gyro_range = s_mpu_config.gyro_range;
    mpu_config.dlpf_bandwidth = s_mpu_config.dlpf_bandwidth;
    mpu_config.sample_rate_hz = s_mpu_config.sample_rate_hz;
    mpu_config.enable_fifo = s_mpu_config.enable_fifo;

    /* Khởi tạo driver MPU6050 */
    esp_err_t ret = mpu6050_init(&mpu_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize MPU6050");
        return ret;
    }

    /* Calibrate nếu được cấu hình (nên bật để có kết quả chính xác) */
    if (s_mpu_config.enable_calibration)
    {
        vTaskDelay(pdMS_TO_TICKS(500));  /* Chờ MPU6050 ổn định */
        mpu6050_calibrate();
    }

    s_initialized = true;
    ESP_LOGI(TAG, "MPU Manager initialized successfully");

    /* Khởi tạo module phát hiện té ngã với cấu hình mặc định */
    fall_config_t fall_config = fall_get_default_config();
    fall_detection_init(&fall_config);

    return ESP_OK;
}

/**
 * @brief Khởi tạo MPU Manager với cấu hình mặc định
 * =================================================
 * @return esp_err_t ESP_OK nếu thành công
 * 
 * Cấu hình mặc định (từ mpu_get_default_config):
 * - I2C: port 0, SDA=21, SCL=22, 400kHz
 * - Accel: ±2g, Gyro: ±250 deg/s
 * - DLPF: 21Hz, sample rate: 100Hz
 * - Read interval: 10ms (100Hz)
 * - Calibration: ENABLED
 * 
 * Đây là cấu hình khuyến nghị cho hầu hết ứng dụng
 */
esp_err_t mpu_manager_init_default(void)
{
    mpu_config_t config = mpu_get_default_config();
    return mpu_manager_init(&config);
}

/**
 * @brief Giải phóng tài nguyên và tắt MPU Manager
 * ================================================
 * @return esp_err_t 
 * 
 * Các bước:
 * 1. Kiểm tra đã khởi tạo
 * 2. Dừng monitoring (nếu đang chạy)
 * 3. Deinit driver MPU6050
 * 4. Reset cờ initialized
 */
esp_err_t mpu_manager_deinit(void)
{
    if (!s_initialized)
    {
        return ESP_OK;
    }

    mpu_manager_stop_monitoring();  /* Dừng task đọc nếu đang chạy */
    mpu6050_deinit();               /* Tắt MPU6050 và giải phóng I2C */

    s_initialized = false;
    ESP_LOGI(TAG, "MPU Manager deinitialized");

    return ESP_OK;
}

/**
 * @brief Lấy dữ liệu MPU6050 một lần (chế độ thủ công)
 * =====================================================
 * @param data  Con trỏ đến struct để nhận dữ liệu
 * @return esp_err_t
 * 
 * Sử dụng khi không muốn chạy continuous monitoring
 * Phù hợp cho các ứng dụng low-power hoặc polling theo nhu cầu
 * 
 * Ví dụ:
 *   mpu6050_data_t data;
 *   mpu_manager_get_data(&data);
 *   printf("Pitch: %.1f\n", data.pitch);
 */
esp_err_t mpu_manager_get_data(mpu6050_data_t *data)
{
    if (!s_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = mpu6050_read(data);
    if (ret == ESP_OK)
    {
        mpu6050_calc_angles(data);  /* Tự động tính góc */
    }

    return ret;
}

/**
 * @brief Bắt đầu giám sát liên tục (chế độ real-time)
 * ====================================================
 * @param callback  Hàm callback để xử lý dữ liệu (NULL để dùng mặc định)
 * 
 * Chức năng:
 * - Tạo task FreeRTOS đọc dữ liệu theo chu kỳ
 * - Nếu callback = NULL, sử dụng default_callback (tích hợp fall detection)
 * - Nếu callback được cung cấp, ghi đè callback mặc định
 * 
 * Ứng dụng:
 * - Phát hiện té ngã real-time
 * - Điều khiển robot theo chuyển động
 * - Ghi log dữ liệu cảm biến
 * 
 * Lưu ý: Chỉ nên gọi một lần, tránh tạo nhiều task
 */
void mpu_manager_start_monitoring(mpu_data_callback_t callback)
{
    if (!s_initialized)
    {
        ESP_LOGE(TAG, "MPU not initialized");
        return;
    }

    if (s_read_task_handle != NULL)
    {
        ESP_LOGW(TAG, "Monitoring already running");
        return;
    }

    /* Nếu callback = NULL, dùng callback mặc định (có fall detection) */
    s_data_callback = callback ? callback : default_callback;
    s_sample_interval_ms = s_mpu_config.read_interval_ms;

    ESP_LOGI(TAG, "Starting monitoring with interval: %d ms", s_sample_interval_ms);

    /**
     * Tạo task với các tham số:
     * - Task function: continuous_read_task
     * - Task name: "mpu_read"
     * - Stack size: 4096 bytes (đủ cho hoạt động I2C và tính toán)
     * - Parameters: NULL
     * - Priority: 5 (cao hơn task idle, đảm bảo đọc đúng chu kỳ)
     * - Task handle: &s_read_task_handle
     */
    xTaskCreate(continuous_read_task, "mpu_read", 4096, NULL, 5, &s_read_task_handle);
}

/**
 * @brief Dừng giám sát liên tục
 * ==============================
 * Xóa task đọc dữ liệu và giải phóng tài nguyên
 * Có thể gọi lại mpu_manager_start_monitoring() sau khi dừng
 */
void mpu_manager_stop_monitoring(void)
{
    if (s_read_task_handle)
    {
        vTaskDelete(s_read_task_handle);
        s_read_task_handle = NULL;
        s_data_callback = NULL;  /* Reset callback */
        ESP_LOGI(TAG, "Stopped monitoring");
    }
}

/**
 * @brief Lấy con trỏ đến cấu hình hiện tại
 * =========================================
 * @return const mpu_config_t*  Con trỏ hằng đến cấu hình
 * 
 * Hữu ích để kiểm tra cấu hình đang chạy
 * Không nên sửa đổi trực tiếp qua con trỏ này
 */
const mpu_config_t *mpu_manager_get_config(void)
{
    return &s_mpu_config;
}

/**
 * @brief Kiểm tra trạng thái khởi tạo của MPU Manager
 * ====================================================
 * @return true nếu đã khởi tạo thành công
 */
bool mpu_manager_is_initialized(void)
{
    return s_initialized;
}

/**
 * @brief Kiểm tra xem monitoring có đang chạy không
 * =================================================
 * @return true nếu đang monitoring (task đang chạy)
 */
bool mpu_manager_is_monitoring(void)
{
    return (s_read_task_handle != NULL);
}