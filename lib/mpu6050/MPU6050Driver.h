/**
 * @file MPU6050Driver.h
 * @brief Driver cho cảm biến MPU6050 (Gia tốc kế + Con quay hồi chuyển)
 */

#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// ==================== REGISTER MAP ====================
// Các thanh ghi của MPU6050 (từ datasheet)
#define MPU6050_ADDR 0x68     /*!< Địa chỉ I2C mặc định (AD0 = 0) */
#define MPU6050_ADDR_ALT 0x69 /*!< Địa chỉ thay thế (AD0 = 1) */

#define MPU6050_REG_SMPLRT_DIV 0x19   /*!< Sample rate divider */
#define MPU6050_REG_CONFIG 0x1A       /*!< Cấu hình */
#define MPU6050_REG_GYRO_CONFIG 0x1B  /*!< Cấu hình con quay */
#define MPU6050_REG_ACCEL_CONFIG 0x1C /*!< Cấu hình gia tốc */
#define MPU6050_REG_FIFO_EN 0x23      /*!< FIFO enable */
#define MPU6050_REG_INT_PIN_CFG 0x37  /*!< Interrupt pin config */
#define MPU6050_REG_INT_ENABLE 0x38   /*!< Interrupt enable */
#define MPU6050_REG_INT_STATUS 0x3A   /*!< Interrupt status */

// Thanh ghi dữ liệu
#define MPU6050_REG_ACCEL_XOUT_H 0x3B /*!< Gia tốc X cao byte */
#define MPU6050_REG_ACCEL_XOUT_L 0x3C /*!< Gia tốc X thấp byte */
#define MPU6050_REG_ACCEL_YOUT_H 0x3D /*!< Gia tốc Y cao byte */
#define MPU6050_REG_ACCEL_YOUT_L 0x3E /*!< Gia tốc Y thấp byte */
#define MPU6050_REG_ACCEL_ZOUT_H 0x3F /*!< Gia tốc Z cao byte */
#define MPU6050_REG_ACCEL_ZOUT_L 0x40 /*!< Gia tốc Z thấp byte */
#define MPU6050_REG_TEMP_OUT_H 0x41   /*!< Nhiệt độ cao byte */
#define MPU6050_REG_TEMP_OUT_L 0x42   /*!< Nhiệt độ thấp byte */
#define MPU6050_REG_GYRO_XOUT_H 0x43  /*!< Con quay X cao byte */
#define MPU6050_REG_GYRO_XOUT_L 0x44  /*!< Con quay X thấp byte */
#define MPU6050_REG_GYRO_YOUT_H 0x45  /*!< Con quay Y cao byte */
#define MPU6050_REG_GYRO_YOUT_L 0x46  /*!< Con quay Y thấp byte */
#define MPU6050_REG_GYRO_ZOUT_H 0x47  /*!< Con quay Z cao byte */
#define MPU6050_REG_GYRO_ZOUT_L 0x48  /*!< Con quay Z thấp byte */

#define MPU6050_REG_PWR_MGMT_1 0x6B /*!< Quản lý nguồn 1 */
#define MPU6050_REG_PWR_MGMT_2 0x6C /*!< Quản lý nguồn 2 */
#define MPU6050_REG_WHO_AM_I 0x75   /*!< Who am I (trả về 0x68) */

// ==================== SCALE FACTORS ====================
// Hệ số chuyển đổi tùy theo cấu hình
#define ACCEL_SCALE_2G 16384.0f /*!< LSB/g cho thang đo ±2g */
#define ACCEL_SCALE_4G 8192.0f  /*!< LSB/g cho thang đo ±4g */
#define ACCEL_SCALE_8G 4096.0f  /*!< LSB/g cho thang đo ±8g */
#define ACCEL_SCALE_16G 2048.0f /*!< LSB/g cho thang đo ±16g */

#define GYRO_SCALE_250 131.0f /*!< LSB/°/s cho thang đo ±250°/s */
#define GYRO_SCALE_500 65.5f  /*!< LSB/°/s cho thang đo ±500°/s */
#define GYRO_SCALE_1000 32.8f /*!< LSB/°/s cho thang đo ±1000°/s */
#define GYRO_SCALE_2000 16.4f /*!< LSB/°/s cho thang đo ±2000°/s */

#define TEMP_SCALE 340.0f  /*!< LSB/°C */
#define TEMP_OFFSET 36.53f /*!< Offset nhiệt độ */

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo MPU6050
 * @return true nếu thành công, false nếu thất bại
 */
bool mpu6050_init(void);

/**
 * @brief Đọc dữ liệu từ MPU6050
 * @param data Con trỏ tới struct để lưu dữ liệu
 * @return true nếu đọc thành công
 */
bool mpu6050_read(mpu6050_data_t *data);

/**
 * @brief Kiểm tra MPU6050 có hoạt động không
 * @return true nếu cảm biến sẵn sàng
 */
bool mpu6050_is_available(void);

/**
 * @brief Đặt thang đo gia tốc
 * @param scale 0:±2g, 1:±4g, 2:±8g, 3:±16g
 * @return true nếu thành công
 */
bool mpu6050_set_accel_scale(uint8_t scale);

/**
 * @brief Đặt thang đo con quay
 * @param scale 0:±250, 1:±500, 2:±1000, 3:±2000
 * @return true nếu thành công
 */
bool mpu6050_set_gyro_scale(uint8_t scale);

/**
 * @brief Đưa MPU6050 vào chế độ ngủ
 */
void mpu6050_sleep(void);

/**
 * @brief Đánh thức MPU6050
 */
void mpu6050_wakeup(void);

#endif // MPU6050_DRIVER_H