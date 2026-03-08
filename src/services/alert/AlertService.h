/**
 * @file AlertService.h
 * @brief Service xử lý cảnh báo khi phát hiện té ngã
 */

#ifndef ALERT_SERVICE_H
#define ALERT_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// ==================== CONFIG ====================
#define ALERT_MAX_QUEUE 5         /*!< Số lượng cảnh báo tối đa trong hàng đợi */
#define ALERT_RETRY_COUNT 3       /*!< Số lần thử lại khi gửi thất bại */
#define ALERT_RETRY_DELAY_MS 5000 /*!< Thời gian giữa các lần thử (5s) */

// ==================== FUNCTION PROTOTYPES ====================

/**
 * @brief Khởi tạo Alert Service
 */
void alert_service_init(void);
