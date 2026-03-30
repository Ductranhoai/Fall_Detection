idf_component_register(
    SRCS "mpu6050.c" "fall_detection.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)