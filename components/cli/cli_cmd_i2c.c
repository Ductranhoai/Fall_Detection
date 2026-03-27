#include "esp_console.h"
#include "driver/i2c.h"
#include <stdlib.h>
#include <stdio.h>

#define I2C_MASTER_NUM I2C_NUM_0

static int cmd_i2c_scan(int argc, char **argv)
{
    for (int addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        if (i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS) == ESP_OK) {
            printf("Found: 0x%02X\n", addr);
        }

        i2c_cmd_link_delete(cmd);
    }
    return 0;
}

static int cmd_i2c_read(int argc, char **argv)
{
    uint8_t dev = strtol(argv[1], NULL, 0);
    uint8_t reg = strtol(argv[2], NULL, 0);
    uint8_t data;

    i2c_master_write_read_device(I2C_MASTER_NUM, dev, &reg, 1, &data, 1, 1000 / portTICK_PERIOD_MS);

    printf("0x%02X = 0x%02X\n", reg, data);
    return 0;
}

static int cmd_i2c_write(int argc, char **argv)
{
    uint8_t dev = strtol(argv[1], NULL, 0);
    uint8_t reg = strtol(argv[2], NULL, 0);
    uint8_t data = strtol(argv[3], NULL, 0);

    uint8_t buf[2] = {reg, data};

    i2c_master_write_to_device(I2C_MASTER_NUM, dev, buf, 2, 1000 / portTICK_PERIOD_MS);

    printf("Write OK\n");
    return 0;
}

void cli_register_i2c(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "i2c_scan",
        .help = "Scan I2C bus",
        .func = cmd_i2c_scan,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "i2c_read",
        .help = "i2c_read <addr> <reg>",
        .func = cmd_i2c_read,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "i2c_write",
        .help = "i2c_write <addr> <reg> <data>",
        .func = cmd_i2c_write,
    });
}