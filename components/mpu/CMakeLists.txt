idf_component_register(
    SRCS 
        "mpu6050.c"
        "mpu_manager.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)