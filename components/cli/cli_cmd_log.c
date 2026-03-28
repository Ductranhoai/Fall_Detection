#include "esp_console.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_SIZE 4096

static char log_buffer[LOG_BUFFER_SIZE];
static int log_index = 0;

static vprintf_like_t original_vprintf = NULL;

// Hook function
static int log_vprintf(const char *fmt, va_list args)
{
    char temp[256];
    int len = vsnprintf(temp, sizeof(temp), fmt, args);

    // print ra UART như bình thường
    if (original_vprintf) {
        original_vprintf(fmt, args);
    }

    // lưu vào buffer
    if (len > 0) {
        if (log_index + len >= LOG_BUFFER_SIZE) {
            log_index = 0; // overwrite kiểu vòng
        }
        memcpy(&log_buffer[log_index], temp, len);
        log_index += len;
    }

    return len;
}

// command dmesg
static int cmd_dmesg(int argc, char **argv)
{
    printf("---- dmesg ----\n");

    fwrite(log_buffer, 1, log_index, stdout);

    return 0;
}

void cli_register_log(void)
{
    // hook log
    original_vprintf = esp_log_set_vprintf(log_vprintf);

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "dmesg",
        .help = "Show log buffer",
        .func = cmd_dmesg,
    });
}