#include "cli.h"
#include "fs.h"

#include "wifi_manager.h"
#include "wifi_cli.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void cli_task(void *arg)
{
    cli_start();
}

void app_main(void)
{
    wifi_manager_init();
    fs_init();

    cli_init();

    cli_register_fs();
    cli_register_mem();
    cli_register_i2c();
    wifi_cli_register();

    xTaskCreate(cli_task, "cli", 4096, NULL, 5, NULL);
}