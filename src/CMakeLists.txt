idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        mpu_manager
        mpu6050
        fall_detection
        button
        buzzer
        wifi
        ble
        esp_timer
)
