#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include "uart_console.h"
#include "wifi_manager.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "System starting...");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize WiFi manager
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_LOGI(TAG, "WiFi manager initialized");

    // Initialize UART console
    ESP_ERROR_CHECK(uart_console_init());
    ESP_LOGI(TAG, "UART console initialized");

    // Try to connect with saved credentials (if any) - nhưng không block
    wifi_manager_load_and_connect();

    ESP_LOGI(TAG, "System ready! Use serial monitor to interact.");

    // Main loop chỉ để log và delay
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        // Có thể thêm các task khác ở đây
    }
}