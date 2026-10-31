idf_component_register(
    SRCS "fall_detection.c"
    INCLUDE_DIRS "include"
    REQUIRES mpu6050 esp_timer
)
