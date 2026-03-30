# idf_component_register(
#     SRCS 
#         "cli.c"
#         "cli_cmd_system.c"
#         "cli_cmd_fs.c"
#         "cli_cmd_mem.c"
#         "cli_cmd_i2c.c"
#         "cli_cmd_gpio.c"
#         "cli_cmd_log.c"
#         "cli_cmd_fall.c"
#         "config.c"
#     INCLUDE_DIRS "."
#     REQUIRES console driver esp_common freertos mpu
# )


idf_component_register(
    SRCS 
        "cli.c"
        "cli_cmd_system.c"
        "cli_cmd_fs.c"
        "cli_cmd_mem.c"
        "cli_cmd_i2c.c"
        "cli_cmd_gpio.c"
        "cli_cmd_log.c"
        "cli_cmd_fall.c"
        "config.c"
    INCLUDE_DIRS "."
    REQUIRES console driver esp_common freertos mpu
)

# Thêm flag để bỏ qua warning deprecated
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-deprecated-declarations)