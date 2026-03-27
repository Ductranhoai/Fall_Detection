idf_component_register(
    SRCS 
        "cli.c"
        "cli_cmd_system.c"
        "cli_cmd_fs.c"
        "cli_cmd_mem.c"
        "cli_cmd_i2c.c"
        "cli_cmd_gpio.c"
        "cli_cmd_log.c"
    INCLUDE_DIRS "."
    REQUIRES console driver esp_common freertos
)