#include "cli.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void cli_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};

    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
#pragma GCC diagnostic pop

    esp_console_config_t console_config = {
        .max_cmdline_args = 8,
        .max_cmdline_length = 256,
    };

    esp_console_init(&console_config);

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(50);
}

void cli_start(void)
{
    char *line;

    while (true)
    {
        line = linenoise("esp_32:# ");
        if (!line)
            continue;

        int ret;
        esp_console_run(line, &ret);

        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}

// External declarations
extern void cli_register_system(void);
extern void cli_register_fs(void);
extern void cli_register_mem(void);
extern void cli_register_i2c(void);
extern void cli_register_gpio(void);
extern void cli_register_log(void);
extern void cli_register_mpu(void);
extern void cli_register_fall(void);
extern void wifi_cli_register(void);

static void cli_task(void *arg)
{
    cli_start();
}

void cli_init_all(void)
{
    cli_init();
    cli_register_fs();
    cli_register_mem();
    cli_register_i2c();
    cli_register_gpio();
    cli_register_system();
    cli_register_fall();
    cli_register_mpu();
    wifi_cli_register();

#ifdef CONFIG_CLI_ENABLE_LOG
    cli_register_log();
    printf("LOG commands\n");
#endif
    printf("All commands registered\n\n");
    xTaskCreate(cli_task, "cli", 8192, NULL, 5, NULL);
}