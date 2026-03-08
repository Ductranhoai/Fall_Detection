// /**
//  * @file test_mpu6050_simple.c
//  * @brief Test đơn giản nhất cho MPU6050
//  *
//  * Cách dùng: Copy toàn bộ nội dung này vào file main.c của bạn
//  * Kết nối:
//  * - ESP32 GPIO21 -> SDA
//  * - ESP32 GPIO22 -> SCL
//  * - VCC -> 3.3V
//  * - GND -> GND
//  */

// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/i2c.h"
// #include "esp_log.h"

// // Định nghĩa chân I2C
// #define I2C_MASTER_SCL_IO 22 /*!< GPIO22 dùng làm SCL */
// #define I2C_MASTER_SDA_IO 21 /*!< GPIO21 dùng làm SDA */
// #define I2C_MASTER_NUM I2C_NUM_0
// #define I2C_MASTER_FREQ_HZ 100000
// #define I2C_MASTER_TIMEOUT_MS 1000

// // Địa chỉ MPU6050
// #define MPU6050_ADDR 0x68
// #define MPU6050_WHO_AM_I 0x75 // Thanh ghi WHO_AM_I (trả về 0x68)

// // Tag cho log
// static const char *TAG = "MPU6050_TEST";

// /**
//  * @brief Khởi tạo I2C
//  */
// static void i2c_master_init(void)
// {
//     i2c_config_t conf = {
//         .mode = I2C_MODE_MASTER,
//         .sda_io_num = I2C_MASTER_SDA_IO,
//         .scl_io_num = I2C_MASTER_SCL_IO,
//         .sda_pullup_en = GPIO_PULLUP_ENABLE,
//         .scl_pullup_en = GPIO_PULLUP_ENABLE,
//         .master.clk_speed = I2C_MASTER_FREQ_HZ,
//     };

//     ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
//     ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

//     ESP_LOGI(TAG, "I2C initialized successfully");
// }

// /**
//  * @brief Đọc 1 byte từ MPU6050
//  */
// static esp_err_t mpu6050_read_byte(uint8_t reg_addr, uint8_t *data)
// {
//     // Ghi địa chỉ thanh ghi cần đọc
//     i2c_cmd_handle_t cmd = i2c_cmd_link_create();
//     i2c_master_start(cmd);
//     i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
//     i2c_master_write_byte(cmd, reg_addr, true);
//     i2c_master_stop(cmd);
//     esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
//     i2c_cmd_link_delete(cmd);

//     if (ret != ESP_OK)
//         return ret;

//     // Đọc dữ liệu
//     cmd = i2c_cmd_link_create();
//     i2c_master_start(cmd);
//     i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);
//     i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
//     i2c_master_stop(cmd);
//     ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
//     i2c_cmd_link_delete(cmd);

//     return ret;
// }

// /**
//  * @main
//  */
// void app_main(void)
// {
//     ESP_LOGI(TAG, "======================================");
//     ESP_LOGI(TAG, "MPU6050 Simple Test");
//     ESP_LOGI(TAG, "======================================");

//     // 1. Khởi tạo I2C
//     i2c_master_init();

//     // 2. Kiểm tra kết nối MPU6050
//     uint8_t who_am_i = 0;
//     esp_err_t ret = mpu6050_read_byte(MPU6050_WHO_AM_I, &who_am_i);

//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Không thể kết nối MPU6050! Lỗi I2C: %s", esp_err_to_name(ret));
//         ESP_LOGE(TAG, "Kiểm tra lại dây kết nối:");
//         ESP_LOGE(TAG, " - SDA (GPIO21) nối với SDA của MPU6050");
//         ESP_LOGE(TAG, " - SCL (GPIO22) nối với SCL của MPU6050");
//         ESP_LOGE(TAG, " - VCC nối với 3.3V");
//         ESP_LOGE(TAG, " - GND nối với GND");
//         return;
//     }

//     if (who_am_i == 0x68)
//     {
//         ESP_LOGI(TAG, "✅ KẾT NỐI MPU6050 THÀNH CÔNG!");
//         ESP_LOGI(TAG, "WHO_AM_I = 0x%02x (đúng 0x68)", who_am_i);
//     }
//     else
//     {
//         ESP_LOGE(TAG, "❌ WHO_AM_I không đúng! Nhận 0x%02x, mong đợi 0x68", who_am_i);
//         ESP_LOGE(TAG, "Có thể bạn dùng MPU6050 có địa chỉ khác?");
//         return;
//     }

//     // 3. Thành công, báo hiệu bằng LED (nếu có)
//     ESP_LOGI(TAG, "\n🎉 MPU6050 HOẠT ĐỘNG TỐT!");
//     ESP_LOGI(TAG, "Bạn có thể tiến hành test đọc dữ liệu chi tiết hơn.");

//     // Nhấp nháy LED để báo hiệu (nếu có LED trên GPIO2)
//     gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
//     for (int i = 0; i < 5; i++)
//     {
//         gpio_set_level(GPIO_NUM_2, 1);
//         vTaskDelay(pdMS_TO_TICKS(200));
//         gpio_set_level(GPIO_NUM_2, 0);
//         vTaskDelay(pdMS_TO_TICKS(200));
//     }
// }

/**
 * @file test_mpu6050_full.c
 * @brief Test đầy đủ MPU6050 - đọc gia tốc, con quay, nhiệt độ
 */

#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

// Định nghĩa chân I2C
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

// Địa chỉ MPU6050
#define MPU6050_ADDR 0x68

// Các thanh ghi quan trọng
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_PWR_MGMT_1 0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_GYRO_XOUT_H 0x43
#define MPU6050_TEMP_OUT_H 0x41

// Scale factors
#define ACCEL_SCALE_2G 16384.0f
#define GYRO_SCALE_250 131.0f
#define TEMP_SCALE 340.0f
#define TEMP_OFFSET 36.53f

static const char *TAG = "MPU6050_FULL_TEST";

/**
 * @brief Khởi tạo I2C
 */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

/**
 * @brief Ghi 1 byte vào MPU6050
 */
static esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Đọc nhiều byte từ MPU6050
 */
static esp_err_t mpu6050_read_bytes(uint8_t reg_addr, uint8_t *data, size_t len)
{
    // Ghi địa chỉ thanh ghi
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK)
        return ret;

    // Đọc dữ liệu
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPU6050_ADDR << 1) | I2C_MASTER_READ, true);

    for (int i = 0; i < len; i++)
    {
        i2c_master_read_byte(cmd, &data[i], (i == len - 1) ? I2C_MASTER_LAST_NACK : I2C_MASTER_ACK);
    }

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 * @brief Đọc 2 byte (1 word) và chuyển về giá trị signed 16-bit
 */
static int16_t mpu6050_read_word(uint8_t reg_addr)
{
    uint8_t buf[2];
    esp_err_t ret = mpu6050_read_bytes(reg_addr, buf, 2);

    if (ret != ESP_OK)
    {
        return 0;
    }

    return (int16_t)((buf[0] << 8) | buf[1]);
}

/**
 * @brief Khởi tạo MPU6050
 */
static bool mpu6050_init(void)
{
    ESP_LOGI(TAG, "Khởi tạo MPU6050...");

    // Kiểm tra thiết bị
    uint8_t who_am_i = 0;
    esp_err_t ret = mpu6050_read_bytes(MPU6050_WHO_AM_I, &who_am_i, 1);

    if (ret != ESP_OK || who_am_i != 0x68)
    {
        ESP_LOGE(TAG, "Không tìm thấy MPU6050! WHO_AM_I=0x%02x", who_am_i);
        return false;
    }

    ESP_LOGI(TAG, "Tìm thấy MPU6050, WHO_AM_I=0x%02x", who_am_i);

    // Reset thiết bị
    ret = mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Wake up và chọn clock source (PLL với X gyro)
    ret = mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0x01);

    ESP_LOGI(TAG, "MPU6050 khởi tạo thành công!");
    return true;
}

/**
 * @brief Đọc tất cả dữ liệu từ MPU6050
 */
static void mpu6050_read_all(float *ax, float *ay, float *az,
                             float *gx, float *gy, float *gz,
                             float *temp)
{
    // Đọc dữ liệu thô
    int16_t ax_raw = mpu6050_read_word(MPU6050_ACCEL_XOUT_H);
    int16_t ay_raw = mpu6050_read_word(MPU6050_ACCEL_XOUT_H + 2);
    int16_t az_raw = mpu6050_read_word(MPU6050_ACCEL_XOUT_H + 4);
    int16_t temp_raw = mpu6050_read_word(MPU6050_TEMP_OUT_H);
    int16_t gx_raw = mpu6050_read_word(MPU6050_GYRO_XOUT_H);
    int16_t gy_raw = mpu6050_read_word(MPU6050_GYRO_XOUT_H + 2);
    int16_t gz_raw = mpu6050_read_word(MPU6050_GYRO_XOUT_H + 4);

    // Chuyển đổi sang đơn vị vật lý
    *ax = (float)ax_raw / ACCEL_SCALE_2G;
    *ay = (float)ay_raw / ACCEL_SCALE_2G;
    *az = (float)az_raw / ACCEL_SCALE_2G;

    *gx = (float)gx_raw / GYRO_SCALE_250;
    *gy = (float)gy_raw / GYRO_SCALE_250;
    *gz = (float)gz_raw / GYRO_SCALE_250;

    *temp = ((float)temp_raw / TEMP_SCALE) + TEMP_OFFSET;
}

/**
 * @brief Tính góc nghiêng từ gia tốc
 */
static void calculate_angles(float ax, float ay, float az,
                             float *roll, float *pitch)
{
    // Roll (góc quanh trục X): arctan(ay / az)
    *roll = atan2f(ay, az) * 180.0f / 3.14159f;

    // Pitch (góc quanh trục Y): arctan(-ax / sqrt(ay^2 + az^2))
    *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / 3.14159f;
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "MPU6050 FULL TEST");
    ESP_LOGI(TAG, "========================================");

    // 1. Khởi tạo I2C
    i2c_init();

    // 2. Khởi tạo MPU6050
    if (!mpu6050_init())
    {
        ESP_LOGE(TAG, "Không thể khởi tạo MPU6050. Dừng test.");
        return;
    }

    ESP_LOGI(TAG, "\nBắt đầu đọc dữ liệu...");
    ESP_LOGI(TAG, "Hãy thử lắc, xoay cảm biến để xem sự thay đổi");
    ESP_LOGI(TAG, "----------------------------------------");

    // 3. Đọc dữ liệu liên tục
    int count = 0;
    float ax, ay, az, gx, gy, gz, temp;
    float roll, pitch;

    while (1)
    {
        // Đọc dữ liệu
        mpu6050_read_all(&ax, &ay, &az, &gx, &gy, &gz, &temp);
        calculate_angles(ax, ay, az, &roll, &pitch);

        // Tính gia tốc tổng hợp
        float total_accel = sqrtf(ax * ax + ay * ay + az * az);

        // In kết quả
        printf("\n[%d] ====================\n", ++count);
        printf("📊 GIA TỐC (g):\n");
        printf("   X: %7.2f | Y: %7.2f | Z: %7.2f | Tổng: %.2f\n", ax, ay, az, total_accel);

        printf("🔄 GÓC (độ):\n");
        printf("   Roll: %6.1f | Pitch: %6.1f\n", roll, pitch);

        printf("⚡ CON QUAY (độ/s):\n");
        printf("   X: %7.2f | Y: %7.2f | Z: %7.2f\n", gx, gy, gz);

        printf("🌡️ NHIỆT ĐỘ: %.2f°C\n", temp);

        // Hiển thị trạng thái cảm biến
        if (fabs(total_accel - 1.0f) < 0.1f)
        {
            printf("✅ Cảm biến đang YÊN (1g)\n");
        }
        else if (total_accel < 0.5f)
        {
            printf("⚠️  Cảnh báo: RƠI TỰ DO! (%.2fg)\n", total_accel);
        }
        else if (total_accel > 2.0f)
        {
            printf("⚠️  Cảnh báo: VA CHẠM! (%.2fg)\n", total_accel);
        }

        // Hiệu ứng LED theo gia tốc (nếu có)
        gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
        if (total_accel > 2.0f)
        {
            // Va chạm -> nhấp nháy nhanh
            for (int i = 0; i < 3; i++)
            {
                gpio_set_level(GPIO_NUM_2, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(GPIO_NUM_2, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
        else if (total_accel < 0.5f)
        {
            // Rơi tự do -> sáng liên tục
            gpio_set_level(GPIO_NUM_2, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(GPIO_NUM_2, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Đọc mỗi 500ms
    }
}