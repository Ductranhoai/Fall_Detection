/**
 * Enable :
 * CONFIG_FREERTOS_USE_TRACE_FACILITY=y
 * CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
 */

#include "esp_console.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <stdio.h>

static int cmd_reboot(int argc, char **argv)
{
    printf("Rebooting...\n");
    esp_restart();
    return 0;
}

static int cmd_free(int argc, char **argv)
{
    printf("Free heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    return 0;
}

static int cmd_tasks(int argc, char **argv)
{
    printf("Task list:\n");
    vTaskList(NULL); 
    return 0;
}

void cli_register_system(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "reboot",
        .help = "Restart ESP32",
        .func = cmd_reboot,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "free",
        .help = "Show free heap",
        .func = cmd_free,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "tasks",
        .help = "List FreeRTOS tasks",
        .func = cmd_tasks,
    });
}