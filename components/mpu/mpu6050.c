#include "mpu6050.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>

static const char *TAG = "MPU6050";

/**
 * @brief Biến tĩnh lưu trạng thái của driver
 * ============================================
 * s_i2c_port:       Cổng I2C đang sử dụng (I2C_NUM_0 hoặc I2C_NUM_1)
 * s_initialized:    Cờ kiểm tra đã khởi tạo thành công hay chưa
 * s_config:         Cấu hình hiện tại của MPU6050 (dải đo, tần số...)
 * s_data_callback:  Con trỏ hàm callback khi có dữ liệu mới
 * s_read_task_handle: Handle của task đọc liên tục (FreeRTOS)
 * s_sample_interval_ms: Chu kỳ đọc dữ liệu (ms)
 * s_accel_scale:    Hệ số chia để chuyển raw data -> g (phụ thuộc dải đo)
 * s_gyro_scale:     Hệ số chia để chuyển raw data -> deg/s (phụ thuộc dải đo)
 * s_gyro_bias:      Giá trị bias của gyroscope (cần trừ khi đọc)
 * s_accel_bias:     Giá trị bias của accelerometer (cần trừ khi đọc)
 */
static i2c_port_t s_i2c_port = I2C_NUM_0;
static bool s_initialized = false;
static mpu6050_config_t s_config;
static mpu_data_callback_t s_data_callback = NULL;
static TaskHandle_t s_read_task_handle = NULL;
static uint32_t s_sample_interval_ms = 0;
static float s_accel_scale = 16384.0;  // Mặc định cho dải ±2g (2^15/2 = 16384)
static float s_gyro_scale = 131.0;      // Mặc định cho dải ±250 deg/s
static float s_gyro_bias[3] = {0, 0, 0};    // Bias cho 3 trục gyro (X,Y,Z)
static float s_accel_bias[3] = {0, 0, 0};   // Bias cho 3 trục accel (X,Y,Z)

/* Khai báo trước các hàm nội bộ */
static esp_err_t write_byte(uint8_t reg, uint8_t data);
static esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len);
static esp_err_t set_accel_range(mpu_accel_range_t range);
static esp_err_t set_gyro_range(mpu_gyro_range_t range);
static esp_err_t set_dlpf(mpu_dlpf_bandwidth_t bandwidth);
static esp_err_t set_sample_rate(uint32_t rate_hz);
static void update_scales(void);

/**
 * @brief Ghi 1 byte dữ liệu vào thanh ghi của MPU6050
 * ====================================================
 * @param reg   Địa chỉ thanh ghi (8-bit)
 * @param data  Dữ liệu cần ghi (8-bit)
 * @return esp_err_t ESP_OK nếu thành công
 * 
 * Nguyên lý: I2C master gửi 2 byte: [địa chỉ thanh ghi][dữ liệu]
 * Timeout: 100ms (chuyển đổi từ tick)
 */
static esp_err_t write_byte(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(s_i2c_port, MPU6050_ADDR, buf, 2, pdMS_TO_TICKS(100));
}

/**
 * @brief Đọc nhiều byte từ MPU6050 bắt đầu từ thanh ghi chỉ định
 * ==============================================================
 * @param reg   Địa chỉ thanh ghi bắt đầu
 * @param data  Buffer để lưu dữ liệu đọc được
 * @param len   Số byte cần đọc
 * @return esp_err_t ESP_OK nếu thành công
 * 
 * Nguyên lý: Gửi địa chỉ thanh ghi, sau đó đọc len byte dữ liệu
 */
static esp_err_t read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(s_i2c_port, MPU6050_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

/**
 * @brief Cấu hình dải đo cho accelerometer
 * ==========================================
 * @param range  Dải đo (2g, 4g, 8g hoặc 16g)
 * @return esp_err_t 
 * 
 * Chi tiết:
 * - Thanh ghi ACCEL_CONFIG (0x1C) chứa cấu hình
 * - Bits [4:3] (AFS_SEL) điều khiển dải đo:
 *   00: ±2g, 01: ±4g, 10: ±8g, 11: ±16g
 * - Cập nhật s_accel_scale sau khi cấu hình thành công
 */
static esp_err_t set_accel_range(mpu_accel_range_t range)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_ACCEL_CONFIG, &config, 1);
    if (ret != ESP_OK) return ret;
    
    config &= ~0x18;  /* Xóa 2 bit AFS_SEL (bit 4 và 3) bằng cách AND với 11100111 */
    config |= range;   /* Set giá trị mới */
    
    ret = write_byte(MPU6050_ACCEL_CONFIG, config);
    if (ret != ESP_OK) return ret;
    
    /* Cập nhật hệ số chia dựa trên dải đo */
    switch (range) {
        case MPU_ACCEL_RANGE_2G:
            s_accel_scale = 16384.0;  /* 2^15/2 = 16384 LSB/g */
            break;
        case MPU_ACCEL_RANGE_4G:
            s_accel_scale = 8192.0;   /* 2^15/4 = 8192 LSB/g */
            break;
        case MPU_ACCEL_RANGE_8G:
            s_accel_scale = 4096.0;   /* 2^15/8 = 4096 LSB/g */
            break;
        case MPU_ACCEL_RANGE_16G:
            s_accel_scale = 2048.0;   /* 2^15/16 = 2048 LSB/g */
            break;
    }
    
    return ESP_OK;
}

/**
 * @brief Cấu hình dải đo cho gyroscope
 * ======================================
 * @param range  Dải đo (±250, ±500, ±1000, ±2000 deg/s)
 * @return esp_err_t
 * 
 * Chi tiết:
 * - Thanh ghi GYRO_CONFIG (0x1B)
 * - Bits [4:3] (FS_SEL) điều khiển dải đo
 * - Cập nhật s_gyro_scale tương ứng
 */
static esp_err_t set_gyro_range(mpu_gyro_range_t range)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_GYRO_CONFIG, &config, 1);
    if (ret != ESP_OK) return ret;
    
    config &= ~0x18;  /* Xóa 2 bit FS_SEL */
    config |= range;   /* Set giá trị mới */
    
    ret = write_byte(MPU6050_GYRO_CONFIG, config);
    if (ret != ESP_OK) return ret;
    
    /* Cập nhật hệ số chia (độ nhạy) */
    switch (range) {
        case MPU_GYRO_RANGE_250:
            s_gyro_scale = 131.0;   /* 131 LSB/(deg/s) */
            break;
        case MPU_GYRO_RANGE_500:
            s_gyro_scale = 65.5;    /* 65.5 LSB/(deg/s) */
            break;
        case MPU_GYRO_RANGE_1000:
            s_gyro_scale = 32.8;    /* 32.8 LSB/(deg/s) */
            break;
        case MPU_GYRO_RANGE_2000:
            s_gyro_scale = 16.4;    /* 16.4 LSB/(deg/s) */
            break;
    }
    
    return ESP_OK;
}

/**
 * @brief Cấu hình bộ lọc thông thấp số (DLPF - Digital Low Pass Filter)
 * ======================================================================
 * @param bandwidth  Băng thông cắt của bộ lọc
 * @return esp_err_t
 * 
 * Ý nghĩa: Lọc nhiễu tần số cao, càng nhỏ càng mượt nhưng chậm hơn
 * - Thanh ghi CONFIG (0x1A)
 * - Bits [2:0] (DLPF_CFG) chọn băng thông
 */
static esp_err_t set_dlpf(mpu_dlpf_bandwidth_t bandwidth)
{
    uint8_t config;
    esp_err_t ret = read_bytes(MPU6050_CONFIG, &config, 1);
    if (ret != ESP_OK) return ret;
    
    config &= ~0x07;  /* Xóa 3 bit DLPF_CFG */
    config |= bandwidth; /* Set giá trị mới */
    
    return write_byte(MPU6050_CONFIG, config);
}

/**
 * @brief Cấu hình tần số lấy mẫu (Sample Rate)
 * =============================================
 * @param rate_hz  Tần số mong muốn (Hz)
 * @return esp_err_t
 * 
 * Công thức: Sample Rate = Gyro Output Rate / (1 + SMPLRT_DIV)
 * - Gyro Output Rate = 8kHz khi DLPF bật, 1kHz khi tắt
 * - Code giả định DLPF bật (8kHz)
 * - SMPLRT_DIV = (8000 / rate_hz) - 1
 */
static esp_err_t set_sample_rate(uint32_t rate_hz)
{
    /* Tính toán hệ số chia dựa trên giả định 8kHz internal rate */
    uint8_t divider = (8000 / rate_hz) - 1;
    if (divider > 255) divider = 255;  /* Giới hạn trong 8-bit */
    
    return write_byte(0x19, divider);  /* Thanh ghi SMPLRT_DIV */
}

/**
 * @brief Cập nhật hệ số scale theo cấu hình hiện tại
 * ===================================================
 * Hàm này được gọi để đồng bộ scale factor với dải đo đã cấu hình
 */
static void update_scales(void)
{
    set_accel_range(s_config.accel_range);
    set_gyro_range(s_config.gyro_range);
}

/**
 * @brief Lấy cấu hình mặc định cho MPU6050
 * =========================================
 * @return mpu6050_config_t  Cấu hình với các giá trị khuyến nghị
 * 
 * Cấu hình mặc định:
 * - I2C port 0, SDA=21, SCL=22, 400kHz
 * - Accel ±2g, Gyro ±250 deg/s
 * - DLPF 21Hz, sample rate 100Hz
 * - FIFO disabled
 */
mpu6050_config_t mpu6050_get_default_config(void)
{
    mpu6050_config_t config = {
        .i2c_port = I2C_NUM_0,
        .i2c_freq_hz = 400000,
        .sda_pin = 21,
        .scl_pin = 22,
        .accel_range = MPU_ACCEL_RANGE_2G,
        .gyro_range = MPU_GYRO_RANGE_250,
        .dlpf_bandwidth = MPU_DLPF_BW_21,
        .sample_rate_hz = 100,
        .enable_fifo = false,
        .fifo_buffer_size = 1024
    };
    return config;
}

/**
 * @brief Khởi tạo MPU6050 với cấu hình được chỉ định
 * ===================================================
 * @param config  Con trỏ đến cấu hình
 * @return esp_err_t
 * 
 * Quy trình khởi tạo:
 * 1. Kiểm tra chưa được khởi tạo
 * 2. Cấu hình I2C master
 * 3. Wake up MPU6050 (ghi 0 vào PWR_MGMT_1)
 * 4. Kiểm tra WHO_AM_I (phải là 0x68)
 * 5. Cấu hình dải đo, bộ lọc, tần số
 * 6. Tùy chọn cấu hình FIFO
 * 7. Đánh dấu đã khởi tạo
 */
esp_err_t mpu6050_init(const mpu6050_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "MPU6050 already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Lưu cấu hình */
    memcpy(&s_config, config, sizeof(mpu6050_config_t));
    s_i2c_port = s_config.i2c_port;
    
    ESP_LOGI(TAG, "Initializing MPU6050...");
    ESP_LOGI(TAG, "I2C: port=%d, SDA=%d, SCL=%d, freq=%dHz", 
             s_config.i2c_port, s_config.sda_pin, s_config.scl_pin, s_config.i2c_freq_hz);
    
    /* Cấu hình I2C master */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = s_config.sda_pin,
        .scl_io_num = s_config.scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  /* Bật điện trở kéo lên nội tại */
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = s_config.i2c_freq_hz,
    };
    
    esp_err_t ret = i2c_param_config(s_config.i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(s_config.i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* Wake up MPU6050 từ sleep mode (ghi 0 vào PWR_MGMT_1) */
    ret = write_byte(MPU6050_PWR_MGMT_1, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake up MPU6050");
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  /* Chờ ổn định */
    
    /* Kiểm tra ID thiết bị */
    uint8_t whoami;
    ret = read_bytes(MPU6050_WHO_AM_I, &whoami, 1);
    if (ret != ESP_OK || whoami != 0x68) {
        ESP_LOGE(TAG, "Wrong device ID: 0x%02X (expected 0x68)", whoami);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* Cấu hình cảm biến */
    set_accel_range(s_config.accel_range);
    set_gyro_range(s_config.gyro_range);
    set_dlpf(s_config.dlpf_bandwidth);
    set_sample_rate(s_config.sample_rate_hz);
    
    /* Cấu hình FIFO (First-In-First-Out buffer) nếu cần */
    if (s_config.enable_fifo) {
        uint8_t fifo_config = 0;
        read_bytes(0x23, &fifo_config, 1);  /* Thanh ghi FIFO_EN */
        fifo_config |= (1 << 6);  /* Enable FIFO */
        write_byte(0x23, fifo_config);
        
        write_byte(0x6A, 0x40);  /* Reset FIFO (USER_CTRL register) */
        vTaskDelay(pdMS_TO_TICKS(50));
        write_byte(0x6A, 0x00);  /* Clear reset bit */
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "MPU6050 initialized successfully!");
    return ESP_OK;
}

/**
 * @brief Giải phóng tài nguyên và tắt MPU6050
 * ===========================================
 * @return esp_err_t
 * 
 * Các bước:
 * 1. Dừng task đọc liên tục (nếu đang chạy)
 * 2. Đặt MPU6050 vào sleep mode để tiết kiệm điện
 * 3. Xóa driver I2C
 * 4. Reset cờ initialized
 */
esp_err_t mpu6050_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    mpu6050_stop_continuous_read();
    
    /* Put MPU6050 to sleep: set SLEEP bit (bit 6) của PWR_MGMT_1 */
    write_byte(MPU6050_PWR_MGMT_1, 0x40);
    
    esp_err_t ret = i2c_driver_delete(s_i2c_port);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete I2C driver");
    }
    
    s_initialized = false;
    ESP_LOGI(TAG, "MPU6050 deinitialized");
    return ret;
}

/**
 * @brief Đọc dữ liệu từ MPU6050
 * ==============================
 * @param data  Con trỏ đến struct để lưu dữ liệu
 * @return esp_err_t
 * 
 * Quy trình đọc:
 * 1. Đọc 14 byte từ thanh ghi ACCEL_XOUT_H (0x3B)
 * 2. Parse raw data: accel (6 byte), temp (2 byte), gyro (6 byte)
 * 3. Tính nhiệt độ từ raw: temp = raw/340 + 36.53
 * 4. Trừ bias đã calibrate
 * 5. Chuyển đổi sang đơn vị vật lý (g và deg/s)
 * 6. Ghi timestamp (ms)
 * 
 * Lưu ý: Dữ liệu trả về là signed 16-bit (bù 2)
 */
esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t buf[14];
    
    /* Đọc toàn bộ 14 byte từ thanh ghi đầu tiên */
    esp_err_t ret = read_bytes(MPU6050_ACCEL_XOUT_H, buf, 14);
    if (ret != ESP_OK) return ret;
    
    /* Parse accelerometer (big-endian: high byte first) */
    data->ax = (buf[0] << 8) | buf[1];
    data->ay = (buf[2] << 8) | buf[3];
    data->az = (buf[4] << 8) | buf[5];
    
    /* Parse temperature (2 bytes) và chuyển đổi sang độ C */
    int16_t temp_raw = (buf[6] << 8) | buf[7];
    data->temperature = temp_raw / 340.0 + 36.53;  /* Công thức từ datasheet */
    
    /* Parse gyroscope */
    data->gx = (buf[8] << 8) | buf[9];
    data->gy = (buf[10] << 8) | buf[11];
    data->gz = (buf[12] << 8) | buf[13];
    
    /* Bù trừ bias (đã tính từ calibration) */
    float ax_comp = data->ax - s_accel_bias[0];
    float ay_comp = data->ay - s_accel_bias[1];
    float az_comp = data->az - s_accel_bias[2];
    
    float gx_comp = data->gx - s_gyro_bias[0];
    float gy_comp = data->gy - s_gyro_bias[1];
    float gz_comp = data->gz - s_gyro_bias[2];
    
    /* Chuyển đổi sang đơn vị vật lý */
    data->accel_x = ax_comp / s_accel_scale;
    data->accel_y = ay_comp / s_accel_scale;
    data->accel_z = az_comp / s_accel_scale;
    
    data->gyro_x = gx_comp / s_gyro_scale;
    data->gyro_y = gy_comp / s_gyro_scale;
    data->gyro_z = gz_comp / s_gyro_scale;
    
    /* Lấy timestamp từ hệ thống (microsecond -> millisecond) */
    data->timestamp_ms = esp_timer_get_time() / 1000;
    
    return ESP_OK;
}

/**
 * @brief Tính góc pitch và roll từ dữ liệu accelerometer
 * =======================================================
 * @param data  Con trỏ đến dữ liệu (sẽ được cập nhật góc)
 * @return esp_err_t
 * 
 * Công thức:
 * - Pitch (θ): Góc xoay quanh trục X
 *   θ = atan2(-Ax, sqrt(Ay² + Az²))
 * 
 * - Roll (φ): Góc xoay quanh trục Y  
 *   φ = atan2(Ay, Az)
 * 
 * Kết quả trả về ở đơn vị độ (nhân với 180/π)
 * 
 * Lưu ý: Phép tính này chỉ chính xác khi cảm biến đứng yên,
 * vì gia tốc tổng hợp phải bằng 1g
 */
esp_err_t mpu6050_calc_angles(mpu6050_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;
    
    /* Pitch: Góc nghiêng trước/sau (X axis) */
    data->pitch = atan2(-data->accel_x, 
                        sqrt(data->accel_y * data->accel_y + 
                             data->accel_z * data->accel_z)) * 180.0 / M_PI;
    
    /* Roll: Góc nghiêng trái/phải (Y axis) */
    data->roll = atan2(data->accel_y, data->accel_z) * 180.0 / M_PI;
    
    return ESP_OK;
}

/**
 * @brief Hiệu chỉnh cảm biến (Calibration)
 * =========================================
 * @return esp_err_t
 * 
 * Quy trình:
 * 1. Yêu cầu người dùng giữ cảm biến yên
 * 2. Lấy 200 mẫu dữ liệu (mỗi 10ms, tổng 2 giây)
 * 3. Tính trung bình của raw data để tìm bias
 * 4. Với accelerometer: Điều chỉnh trục Z để khi đứng yên = 1g
 * 
 * Lưu ý: 
 * - Cảm biến phải được đặt yên hoàn toàn khi calibrate
 * - Nên đặt cảm biến trên mặt phẳng nằm ngang
 */
esp_err_t mpu6050_calibrate(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting calibration... Keep sensor still!");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    const int num_samples = 200;  /* Số mẫu lấy trung bình */
    int32_t sum_accel[3] = {0, 0, 0};
    int32_t sum_gyro[3] = {0, 0, 0};
    
    /* Thu thập mẫu */
    for (int i = 0; i < num_samples; i++) {
        mpu6050_data_t data;
        if (mpu6050_read(&data) == ESP_OK) {
            sum_accel[0] += data.ax;
            sum_accel[1] += data.ay;
            sum_accel[2] += data.az;
            sum_gyro[0] += data.gx;
            sum_gyro[1] += data.gy;
            sum_gyro[2] += data.gz;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    /* Tính bias trung bình */
    for (int i = 0; i < 3; i++) {
        s_accel_bias[i] = sum_accel[i] / num_samples;
        s_gyro_bias[i] = sum_gyro[i] / num_samples;
    }
    
    /**
     * Điều chỉnh bias cho accelerometer trục Z
     * ==========================================
     * Khi đứng yên, trục Z phải đọc giá trị tương ứng với 1g
     * Ví dụ: Với dải ±2g, scale=16384 thì giá trị đúng là 16384
     * Bias = giá trị đọc được - giá trị kỳ vọng
     */
    float expected_z = s_accel_scale;  /* Giá trị lý thuyết khi đứng yên */
    s_accel_bias[2] -= expected_z;     /* Điều chỉnh bias trục Z */
    
    ESP_LOGI(TAG, "Calibration complete!");
    ESP_LOGI(TAG, "Accel bias: X=%d, Y=%d, Z=%d", 
             (int)s_accel_bias[0], (int)s_accel_bias[1], (int)s_accel_bias[2]);
    ESP_LOGI(TAG, "Gyro bias: X=%d, Y=%d, Z=%d", 
             (int)s_gyro_bias[0], (int)s_gyro_bias[1], (int)s_gyro_bias[2]);
    
    return ESP_OK;
}

/**
 * @brief Đăng ký hàm callback để nhận dữ liệu liên tục
 * =====================================================
 * @param callback  Con trỏ hàm callback với tham số mpu6050_data_t*
 * 
 * Callback sẽ được gọi mỗi khi có dữ liệu mới trong continuous read mode
 */
void mpu6050_set_data_callback(mpu_data_callback_t callback)
{
    s_data_callback = callback;
}

/**
 * @brief Task đọc dữ liệu liên tục (chạy trong FreeRTOS)
 * =======================================================
 * @param arg  Tham số không sử dụng
 * 
 * Hoạt động:
 * - Vòng lặp vô tận
 * - Đọc dữ liệu và tính góc
 * - Gọi callback nếu được đăng ký
 * - Delay chính xác bằng vTaskDelayUntil
 */
static void continuous_read_task(void *arg)
{
    mpu6050_data_t data;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        if (mpu6050_read(&data) == ESP_OK) {
            mpu6050_calc_angles(&data);  /* Tính góc từ accel */
            
            if (s_data_callback) {
                s_data_callback(&data);  /* Gửi dữ liệu qua callback */
            }
        }
        
        /* Delay chính xác để đạt sample rate mong muốn */
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(s_sample_interval_ms));
    }
}

/**
 * @brief Bắt đầu chế độ đọc liên tục
 * ===================================
 * @param interval_ms  Chu kỳ đọc (millisecond)
 * 
 * Tạo một task FreeRTOS riêng để đọc MPU6050 theo chu kỳ
 * Task có độ ưu tiên 5 (khá cao) và stack 4096 bytes
 */
void mpu6050_start_continuous_read(uint32_t interval_ms)
{
    if (s_read_task_handle != NULL) {
        ESP_LOGW(TAG, "Continuous read already running");
        return;
    }
    
    s_sample_interval_ms = interval_ms;
    
    xTaskCreate(continuous_read_task, "mpu_read", 4096, NULL, 5, &s_read_task_handle);
    ESP_LOGI(TAG, "Started continuous read at %d ms interval", interval_ms);
}

/**
 * @brief Dừng chế độ đọc liên tục
 * =================================
 * Xóa task đọc liên tục và giải phóng handle
 */
void mpu6050_stop_continuous_read(void)
{
    if (s_read_task_handle) {
        vTaskDelete(s_read_task_handle);
        s_read_task_handle = NULL;
        ESP_LOGI(TAG, "Stopped continuous read");
    }
}

/**
 * @brief Kiểm tra trạng thái khởi tạo
 * ====================================
 * @return true nếu đã được khởi tạo thành công
 */
bool mpu6050_is_initialized(void)
{
    return s_initialized;
}