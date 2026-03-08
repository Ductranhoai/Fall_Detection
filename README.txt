form sources

FallDetection_ESP32/
│
├── platformio.ini
│
├── include/                     # Header dùng chung toàn project
│   ├── config.h                 # Cấu hình pin, threshold, enable feature
│   └── types.h                  # struct, enum dùng chung
│
├── lib/                         # Driver / Library độc lập
│
│   ├── mpu6050/
│   │   ├── MPU6050Driver.h
│   │   └── MPU6050Driver.cpp
│   │
│   ├── gps/
│   │   ├── GPSDriver.h
│   │   └── GPSDriver.cpp
│   │
│   ├── fall_detection/
│   │   ├── FallDetector.h
│   │   └── FallDetector.cpp
│
├── src/
│
│   ├── main.cpp                 # Entry point
│
│   ├── app/                     # Application layer (logic chính)
│   │   ├── AppController.h
│   │   └── AppController.cpp
│
│   ├── drivers/                 # Hardware abstraction
│   │   ├── SensorManager.h
│   │   └── SensorManager.cpp
│
│   ├── services/                # Service xử lý logic trung gian
│   │
│   │   ├── fall/
│   │   │   ├── FallService.h
│   │   │   └── FallService.cpp
│   │   │
│   │   ├── alert/
│   │   │   ├── AlertService.h
│   │   │   └── AlertService.cpp
│   │   │
│   │   └── location/
│   │       ├── LocationService.h
│   │       └── LocationService.cpp
│
│   ├── communication/           # Network / communication
│   │
│   │   ├── CommsManager.h
│   │   └── CommsManager.cpp
│   │
│   │   ├── wifi/
│   │   │   ├── WifiManager.h
│   │   │   └── WifiManager.cpp
│   │   │
│   │   ├── ble/
│   │   │   ├── BLEManager.h
│   │   │   └── BLEManager.cpp
│   │   │
│   │   └── sim/
│   │       ├── SIMManager.h
│   │       └── SIMManager.cpp
│
│   ├── system/                  # System level
│   │
│   │   ├── led/
│   │   │   ├── LEDTask.h
│   │   │   └── LEDTask.cpp
│   │   │
│   │   ├── power/
│   │   │   ├── PowerManager.h
│   │   │   └── PowerManager.cpp
│   │   │
│   │   └── logger/
│   │       ├── Logger.h
│   │       └── Logger.cpp
│
└── test/
    ├── test_mpu6050.cpp
    ├── test_gps.cpp
    └── test_fall_detection.cpp