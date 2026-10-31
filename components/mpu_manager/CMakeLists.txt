idf_component_register(
    SRCS "mpu_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES mpu6050 fall_detection esp_timer
)
