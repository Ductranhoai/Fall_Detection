/**
 * @file GPSDriver.c
 * @brief Implementation của driver GPS
 */

#include "GPSDriver.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GPS";

// Buffer cho UART
static uint8_t uart_buffer[GPS_BUF_SIZE];

// Dữ liệu GPS hiện tại
static gps_data_t current_gps_data = {
    .latitude = 0.0,
    .longitude = 0.0,
    .altitude = 0.0,
    .speed = 0.0,
    .course = 0.0,
    .satellites = 0,
    .is_fixed = false,
    .timestamp = 0};

// Buffer cho câu NMEA đang xử lý
static char nmea_sentence[NMEA_MAX_SENTENCE_LEN];
static int nmea_index = 0;

/**
 * @brief Khởi tạo UART cho GPS
 */
static esp_err_t uart_init(void)
{
    // Cấu hình UART
    uart_config_t uart_config = {
        .baud_rate = GPS_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // Cài đặt cấu hình
    esp_err_t err = uart_param_config(GPS_UART_NUM, &uart_config);
    if (err != ESP_OK)
    {
        return err;
    }

    // Cài đặt pin
    err = uart_set_pin(
        GPS_UART_NUM,
        GPS_TX_PIN, // TX pin (ESP32 gửi đi)
        GPS_RX_PIN, // RX pin (ESP32 nhận từ GPS)
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        return err;
    }

    // Cài đặt driver
    return uart_driver_install(
        GPS_UART_NUM,
        GPS_BUF_SIZE * 2, // RX buffer size
        0,                // TX buffer size
        0,                // Queue size
        NULL,             // Queue handle
        0                 // Flags
    );
}

/**
 * @brief Tính checksum của câu NMEA
 * @param sentence Câu NMEA (bao gồm cả $ và *xx)
 * @return true nếu checksum hợp lệ
 */
static bool nmea_checksum_valid(const char *sentence)
{
    if (sentence == NULL || strlen(sentence) < 10)
    {
        return false;
    }

    // Tìm vị trí dấu *
    char *star = strchr(sentence, '*');
    if (star == NULL || strlen(star) < 3)
    {
        return false;
    }

    // Tính checksum: XOR tất cả các ký tự từ sau $ đến trước *
    uint8_t calculated = 0;
    for (const char *p = sentence + 1; p < star; p++)
    {
        calculated ^= *p;
    }

    // Lấy checksum từ câu (2 ký tự hex sau dấu *)
    uint8_t provided = (uint8_t)strtol(star + 1, NULL, 16);

    return calculated == provided;
}

/**
 * @brief Parse câu GPRMC (Recommended Minimum data)
 * Format: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
 */
static void parse_gprmc(const char *sentence)
{
    char temp[100];
    strcpy(temp, sentence);

    char *token;
    int field = 0;

    // Tokenize bằng dấu phẩy
    token = strtok(temp, ",");

    while (token != NULL)
    {
        switch (field)
        {
        case 1: // Time (hhmmss.ss)
            if (strlen(token) >= 6)
            {
                strncpy(current_gps_data.time, token, 9);
                current_gps_data.time[9] = '\0';
            }
            break;

        case 2: // Status (A=active, V=void)
            current_gps_data.is_fixed = (token[0] == 'A');
            break;

        case 3: // Latitude (ddmm.mmmm)
            if (current_gps_data.is_fixed && strlen(token) > 0)
            {
                double lat = atof(token);
                int degrees = (int)(lat / 100);
                double minutes = lat - (degrees * 100);
                current_gps_data.latitude = degrees + (minutes / 60.0);
            }
            break;

        case 4: // N/S indicator
            if (current_gps_data.is_fixed && token[0] == 'S')
            {
                current_gps_data.latitude = -current_gps_data.latitude;
            }
            break;

        case 5: // Longitude (dddmm.mmmm)
            if (current_gps_data.is_fixed && strlen(token) > 0)
            {
                double lon = atof(token);
                int degrees = (int)(lon / 100);
                double minutes = lon - (degrees * 100);
                current_gps_data.longitude = degrees + (minutes / 60.0);
            }
            break;

        case 6: // E/W indicator
            if (current_gps_data.is_fixed && token[0] == 'W')
            {
                current_gps_data.longitude = -current_gps_data.longitude;
            }
            break;

        case 7: // Speed over ground (knots)
            if (current_gps_data.is_fixed && strlen(token) > 0)
            {
                // Convert knots to km/h (1 knot = 1.852 km/h)
                current_gps_data.speed = atof(token) * 1.852f;
            }
            break;

        case 8: // Course over ground (degrees)
            if (current_gps_data.is_fixed && strlen(token) > 0)
            {
                current_gps_data.course = atof(token);
            }
            break;

        case 9: // Date (ddmmyy)
            if (strlen(token) == 6)
            {
                // Convert ddmmyy to dd/mm/yyyy format
                char date_str[11];
                snprintf(date_str, sizeof(date_str),
                         "%c%c/%c%c/20%c%c",
                         token[0], token[1], token[2], token[3],
                         token[4], token[5]);
                strcpy(current_gps_data.date, date_str);
            }
            break;
        }

        token = strtok(NULL, ",");
        field++;
    }

    if (current_gps_data.is_fixed)
    {
        current_gps_data.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_LOGD(TAG, "GPS fixed: %.6f, %.6f",
                 current_gps_data.latitude, current_gps_data.longitude);
    }
}

/**
 * @brief Parse câu GPGGA (Global Positioning System Fix Data)
 * Format: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
 */
static void parse_gpgga(const char *sentence)
{
    char temp[100];
    strcpy(temp, sentence);

    char *token;
    int field = 0;

    token = strtok(temp, ",");

    while (token != NULL)
    {
        switch (field)
        {
        case 6: // Fix quality (0=invalid, 1=GPS fix, 2=DGPS fix)
            current_gps_data.is_fixed = (token[0] != '0');
            break;

        case 7: // Number of satellites
            current_gps_data.satellites = atoi(token);
            break;

        case 9: // Altitude
            if (current_gps_data.is_fixed && strlen(token) > 0)
            {
                current_gps_data.altitude = atof(token);
            }
            break;
        }

        token = strtok(NULL, ",");
        field++;
    }
}

/**
 * @brief Xử lý một câu NMEA hoàn chỉnh
 */
static void process_nmea_sentence(const char *sentence)
{
    // Kiểm tra checksum
    if (!nmea_checksum_valid(sentence))
    {
        ESP_LOGW(TAG, "Invalid checksum: %s", sentence);
        return;
    }

    // Xác định loại câu và parse tương ứng
    if (strncmp(sentence, NMEA_GPRMC, 6) == 0)
    {
        parse_gprmc(sentence);
    }
    else if (strncmp(sentence, NMEA_GPGGA, 6) == 0)
    {
        parse_gpgga(sentence);
    }
    // Có thể thêm các loại câu khác nếu cần
}

bool gps_init(void)
{
    ESP_LOGI(TAG, "Initializing GPS module...");

    // Khởi tạo UART
    esp_err_t err = uart_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "UART init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Reset dữ liệu
    memset(&current_gps_data, 0, sizeof(gps_data_t));
    nmea_index = 0;
    memset(nmea_sentence, 0, sizeof(nmea_sentence));

    ESP_LOGI(TAG, "GPS initialized");
    return true;
}

void gps_update(void)
{
    // Đọc dữ liệu từ UART
    int len = uart_read_bytes(
        GPS_UART_NUM,
        uart_buffer,
        GPS_BUF_SIZE,
        20 / portTICK_PERIOD_MS // Timeout 20ms
    );

    if (len <= 0)
    {
        return;
    }

    // Xử lý từng ký tự nhận được
    for (int i = 0; i < len; i++)
    {
        char c = (char)uart_buffer[i];

        if (c == '$')
        {
            // Bắt đầu câu mới
            nmea_index = 0;
            nmea_sentence[nmea_index++] = c;
        }
        else if (c == '\n' || c == '\r')
        {
            // Kết thúc câu
            if (nmea_index > 0)
            {
                nmea_sentence[nmea_index] = '\0';

                // Xử lý câu NMEA
                process_nmea_sentence(nmea_sentence);

                nmea_index = 0;
            }
        }
        else
        {
            // Thêm ký tự vào câu hiện tại
            if (nmea_index < NMEA_MAX_SENTENCE_LEN - 1)
            {
                nmea_sentence[nmea_index++] = c;
            }
            else
            {
                // Tràn buffer, reset
                nmea_index = 0;
            }
        }
    }
}

bool gps_get_data(gps_data_t *data)
{
    if (data == NULL)
    {
        return false;
    }

    // Copy dữ liệu hiện tại
    memcpy(data, &current_gps_data, sizeof(gps_data_t));

    return current_gps_data.is_fixed;
}

bool gps_has_fix(void)
{
    return current_gps_data.is_fixed;
}

int gps_get_satellites(void)
{
    return current_gps_data.satellites;
}

void gps_get_coordinate_string(char *buffer, size_t buflen)
{
    if (buffer == NULL || buflen < 30)
    {
        return;
    }

    if (current_gps_data.is_fixed)
    {
        snprintf(buffer, buflen, "%.6f, %.6f (alt: %.1fm)",
                 current_gps_data.latitude,
                 current_gps_data.longitude,
                 current_gps_data.altitude);
    }
    else
    {
        snprintf(buffer, buflen, "No GPS fix (sat: %d)",
                 current_gps_data.satellites);
    }
}

void gps_get_maps_link(char *buffer, size_t buflen)
{
    if (buffer == NULL || buflen < 100)
    {
        return;
    }

    if (current_gps_data.is_fixed)
    {
        snprintf(buffer, buflen,
                 "https://maps.google.com/?q=%.6f,%.6f",
                 current_gps_data.latitude,
                 current_gps_data.longitude);
    }
    else
    {
        snprintf(buffer, buflen, "Location not available");
    }
}