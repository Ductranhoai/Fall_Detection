#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "wifi_manager.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE 256
#define CMD_MAX_LEN 128
#define PROMPT "esp> "

static const char *TAG = "uart_console";
static TaskHandle_t uart_task_handle = NULL;

// Lưu thông tin WiFi vào NVS
static esp_err_t save_wifi_config(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK)
    {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    return err;
}

// Xử lý lệnh từ UART
static void handle_command(char *cmd)
{
    // Xóa khoảng trắng đầu cuối
    char *end = cmd + strlen(cmd) - 1;
    while (end > cmd && (*end == '\r' || *end == '\n'))
    {
        *end = '\0';
        end--;
    }

    ESP_LOGI(TAG, "Processing command: '%s'", cmd);

    // Command format: wifi_connect <SSID> <PASSWORD>
    if (strncmp(cmd, "wifi_connect", 12) == 0)
    {
        char ssid[32] = {0};
        char password[64] = {0};

        // Parse command
        int parsed = sscanf(cmd, "wifi_connect %31s %63s", ssid, password);

        if (parsed == 2)
        {
            char msg[100];
            snprintf(msg, sizeof(msg), "\r\n📡 Connecting to SSID: %s\r\n", ssid);
            uart_write_bytes(UART_NUM, msg, strlen(msg));

            // Lưu vào NVS
            esp_err_t err = save_wifi_config(ssid, password);
            if (err == ESP_OK)
            {
                uart_write_bytes(UART_NUM, "✅ Credentials saved. Connecting...\r\n", 37);

                // Kết nối WiFi với thông tin mới
                wifi_manager_connect(ssid, password);
            }
            else
            {
                uart_write_bytes(UART_NUM, "❌ Failed to save WiFi credentials\r\n", 36);
            }
        }
        else
        {
            uart_write_bytes(UART_NUM, "\r\n📝 Usage: wifi_connect <SSID> <PASSWORD>\r\n", 46);
        }
    }
    else if (strcmp(cmd, "help") == 0)
    {
        const char *help_msg =
            "\r\n"
            "╔════════════════════════════════════════════╗\r\n"
            "║           AVAILABLE COMMANDS               ║\r\n"
            "╠════════════════════════════════════════════╣\r\n"
            "║ wifi_connect <SSID> <PASS> - Connect WiFi  ║\r\n"
            "║ wifi_status                 - Show status  ║\r\n"
            "║ wifi_disconnect              - Disconnect   ║\r\n"
            "║ wifi_scan                    - Scan networks║\r\n"
            "║ help                         - This help    ║\r\n"
            "║ restart                      - Restart ESP32║\r\n"
            "║ clear                        - Clear screen ║\r\n"
            "╚════════════════════════════════════════════╝\r\n";
        uart_write_bytes(UART_NUM, help_msg, strlen(help_msg));
    }
    else if (strcmp(cmd, "wifi_status") == 0)
    {
        wifi_ap_record_t ap_info;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);

        // Gửi từng phần thay vì dùng snprintf lớn
        if (err == ESP_OK)
        {
            uart_write_bytes(UART_NUM, "\r\n", 2);
            uart_write_bytes(UART_NUM, "╔════════════════════════════════╗\r\n", 36);
            uart_write_bytes(UART_NUM, "║      WiFi STATUS: CONNECTED    ║\r\n", 37);
            uart_write_bytes(UART_NUM, "╠════════════════════════════════╣\r\n", 36);

            char line[64];
            snprintf(line, sizeof(line), "║ SSID    : %-24s ║\r\n", ap_info.ssid);
            uart_write_bytes(UART_NUM, line, strlen(line));

            snprintf(line, sizeof(line), "║ RSSI    : %-24d ║\r\n", ap_info.rssi);
            uart_write_bytes(UART_NUM, line, strlen(line));

            snprintf(line, sizeof(line), "║ Channel : %-24d ║\r\n", ap_info.primary);
            uart_write_bytes(UART_NUM, line, strlen(line));

            uart_write_bytes(UART_NUM, "╚════════════════════════════════╝\r\n", 36);
        }
        else if (err == ESP_ERR_WIFI_NOT_CONNECT)
        {
            uart_write_bytes(UART_NUM, "\r\n", 2);
            uart_write_bytes(UART_NUM, "╔════════════════════════════════╗\r\n", 36);
            uart_write_bytes(UART_NUM, "║    WiFi STATUS: DISCONNECTED   ║\r\n", 37);
            uart_write_bytes(UART_NUM, "╚════════════════════════════════╝\r\n", 36);
        }
        else
        {
            char msg[100];
            snprintf(msg, sizeof(msg), "\r\n⚠️ WiFi Status: ERROR (%s)\r\n", esp_err_to_name(err));
            uart_write_bytes(UART_NUM, msg, strlen(msg));
        }
    }
    else if (strcmp(cmd, "wifi_disconnect") == 0)
    {
        esp_err_t err = wifi_manager_disconnect();
        if (err == ESP_OK)
        {
            uart_write_bytes(UART_NUM, "\r\n✅ WiFi disconnected\r\n", 22);
        }
        else
        {
            uart_write_bytes(UART_NUM, "\r\n❌ Failed to disconnect\r\n", 25);
        }
    }
    else if (strcmp(cmd, "wifi_scan") == 0)
    {
        uart_write_bytes(UART_NUM, "\r\n📡 Scanning WiFi networks...\r\n", 31);
        // TODO: Implement scan
        uart_write_bytes(UART_NUM, "⚠️ Not implemented yet\r\n", 24);
    }
    else if (strcmp(cmd, "clear") == 0)
    {
        // Clear screen
        uart_write_bytes(UART_NUM, "\033[2J\033[H", 7);
    }
    else if (strcmp(cmd, "restart") == 0)
    {
        uart_write_bytes(UART_NUM, "\r\n🔄 Restarting in 1 second...\r\n", 31);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if (strlen(cmd) > 0)
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "\r\n❓ Unknown command: '%s'\r\nType 'help' for available commands\r\n", cmd);
        uart_write_bytes(UART_NUM, msg, strlen(msg));
    }
}

// Task xử lý UART
static void uart_task(void *pvParameters)
{
    uint8_t *data = malloc(BUF_SIZE);
    char cmd_line[CMD_MAX_LEN] = {0};
    int cmd_index = 0;

    // Đợi UART ổn định
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Test UART
    const char *test = "\r\n*** UART TASK STARTED ***\r\n";
    uart_write_bytes(UART_NUM, test, strlen(test));

    // Welcome message
    const char *welcome =
        "\r\n"
        "╔════════════════════════════════════╗\r\n"
        "║      ESP32 UART CONSOLE v1.0       ║\r\n"
        "║                                     ║\r\n"
        "║  Type 'help' for available commands ║\r\n"
        "╚════════════════════════════════════╝\r\n"
        "\r\n";

    uart_write_bytes(UART_NUM, welcome, strlen(welcome));
    uart_write_bytes(UART_NUM, PROMPT, strlen(PROMPT));

    while (1)
    {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));

        if (len > 0)
        {
            for (int i = 0; i < len; i++)
            {
                char c = data[i];

                if (c == '\r' || c == '\n')
                {
                    // Enter pressed
                    if (cmd_index > 0)
                    {
                        cmd_line[cmd_index] = '\0';
                        uart_write_bytes(UART_NUM, "\r\n", 2);
                        handle_command(cmd_line);
                        cmd_index = 0;
                        memset(cmd_line, 0, CMD_MAX_LEN);
                    }
                    // Print new prompt
                    uart_write_bytes(UART_NUM, PROMPT, strlen(PROMPT));
                }
                else if (c == '\b' || c == 127)
                {
                    // Backspace
                    if (cmd_index > 0)
                    {
                        cmd_index--;
                        cmd_line[cmd_index] = '\0';
                        uart_write_bytes(UART_NUM, "\b \b", 3);
                    }
                }
                else if (c >= 32 && c <= 126)
                {
                    // Normal character - echo
                    if (cmd_index < CMD_MAX_LEN - 1)
                    {
                        cmd_line[cmd_index++] = c;
                        uart_write_bytes(UART_NUM, (const char *)&c, 1);
                    }
                }
            }
        }
    }

    free(data);
    vTaskDelete(NULL);
}

// Khởi tạo UART console
esp_err_t uart_console_init(void)
{
    // Cấu hình UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // Install UART driver
    esp_err_t err = uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        return err;
    }

    // Configure UART parameters
    err = uart_param_config(UART_NUM, &uart_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(err));
        return err;
    }

    // Set UART pins (using default pins)
    err = uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        return err;
    }

    // Create UART task với priority cao hơn
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, &uart_task_handle);

    ESP_LOGI(TAG, "UART console initialized on UART_NUM_0 at 115200 baud");
    return ESP_OK;
}