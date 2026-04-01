idf_component_register(
    SRCS 
        "mpu6050.c"
        "mpu_manager.c"
        "fall_detection.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)