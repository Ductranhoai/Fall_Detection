/**
 * @file GPSDriver.h
 * @brief Driver cho module GPS NEO-6M (dùng UART, parse NMEA)
 */

#ifndef GPS_DRIVER_H
#define GPS_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// ==================== NMEA SENTENCE TYPES ====================
#define NMEA_MAX_SENTENCE_LEN 100 /*!< Độ dài tối đa 1 câu NMEA */
#define NMEA_MIN_SENTENCE_LEN 10  /*!< Độ dài tối thiểu */

// Các loại câu NMEA chúng ta quan tâm
#define NMEA_GPGGA "$GPGGA" /*!< Global Positioning System Fix Data */
#define NMEA_GPGLL "$GPGLL" /*!< Geographic Position - Latitude/Longitude */
#define NMEA_GPGSA "$GPGSA" /*!< GPS DOP and active satellites */
#define NMEA_GPGSV "$GPGSV" /*!< GPS Satellites in view */
#define NMEA_GPRMC "$GPRMC" /*!< Recommended Minimum Specific GPS/Transit */
#define NMEA_GPVTG "$GPVTG" /*!< Track made good and ground speed */

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo GPS module
 * @return true nếu thành công
 */
bool gps_init(void);

/**
 * @brief Cập nhật dữ liệu GPS (đọc từ UART và parse)
 * Hàm này nên được gọi thường xuyên trong loop
 */
void gps_update(void);

/**
 * @brief Lấy dữ liệu GPS mới nhất
 * @param data Con trỏ tới struct để lưu dữ liệu
 * @return true nếu có dữ liệu mới
 */
bool gps_get_data(gps_data_t *data);

/**
 * @brief Kiểm tra GPS đã fix được tọa độ chưa
 * @return true nếu đã fix (có tọa độ hợp lệ)
 */
bool gps_has_fix(void);

/**
 * @brief Lấy số vệ tinh đang kết nối
 * @return số vệ tinh
 */
int gps_get_satellites(void);

/**
 * @brief Lấy tọa độ hiện tại (dạng string để gửi SMS/display)
 * @param buffer Buffer để lưu string
 * @param buflen Kích thước buffer
 */
void gps_get_coordinate_string(char *buffer, size_t buflen);

/**
 * @brief Tạo link Google Maps từ tọa độ
 * @param buffer Buffer để lưu link
 * @param buflen Kích thước buffer
 */
void gps_get_maps_link(char *buffer, size_t buflen);

#endif // GPS_DRIVER_H