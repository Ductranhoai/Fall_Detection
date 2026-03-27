#include "esp_console.h"
#include "driver/gpio.h"
#include <stdlib.h>
#include <stdio.h>

static int cmd_gpio_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: gpio_set <pin> <0|1>\n");
        return 1;
    }

    int pin = atoi(argv[1]);
    int level = atoi(argv[2]);

    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);

    printf("GPIO %d = %d\n", pin, level);
    return 0;
}

static int cmd_gpio_get(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: gpio_get <pin>\n");
        return 1;
    }

    int pin = atoi(argv[1]);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    int level = gpio_get_level(pin);
    printf("GPIO %d = %d\n", pin, level);
    return 0;
}

void cli_register_gpio(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "gpio_set",
        .help = "Set GPIO: gpio_set <pin> <0|1>",
        .func = cmd_gpio_set,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "gpio_get",
        .help = "Get GPIO level",
        .func = cmd_gpio_get,
    });
}