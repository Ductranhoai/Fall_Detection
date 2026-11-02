idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        mpu_manager
        mpu6050
        fall_detection
        max30102
        button
        buzzer
        wifi
        ble
        esp_timer
)
