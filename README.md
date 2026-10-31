# Fall Detection ESP32-C3

Thiết bị đeo/đặt trên người dùng để phát hiện té ngã, báo động bằng còi và gửi thông báo qua BLE. WiFi được dùng để lưu cấu hình mạng và sẵn sàng cho việc kết nối server ở giai đoạn sau.

- Board: ESP32-C3 DevKitM-1
- Framework: ESP-IDF 5.5.3 + PlatformIO (env `esp32-c3`)
- Flash: 4 MB, phân vùng app `factory` 1500 KB (`partitions/partitions_large.csv`)
- FreeRTOS tick: 100 Hz, chu kỳ đọc cảm biến 10 ms (100 Hz)
- Ngôn ngữ log: tiếng Anh, tài liệu: tiếng Việt

## Tính năng

- Đọc MPU6050 qua I2C, phát hiện té ngã theo máy trạng thái 4 bước.
- Còi báo động SOS trên GPIO0, tắt ngay khi bấm nút.
- Nút nhấn GPIO9 dùng ngắt, chống rung 30 ms; giữ 5 giây để bật còi thủ công.
- BLE (NimBLE) với Nordic UART Service: gửi sự kiện, nhận lệnh cấu hình WiFi.
- WiFi STA: tự kết nối lại mạng đã lưu, retry tối đa 3 lần, chỉ lưu mật khẩu khi đã có IP.
- Cấu hình WiFi qua BLE, không cần sửa code hay nạp lại firmware.

## Phần cứng và đấu dây

| Linh kiện | Chân ESP32-C3 | Ghi chú |
|---|---|---|
| MPU6050 VCC | 3V3 | Không dùng 5V |
| MPU6050 GND | GND | |
| MPU6050 SDA | GPIO5 | Có pull-up nội bộ |
| MPU6050 SCL | GPIO4 | Có pull-up nội bộ, I2C 100 kHz |
| Còi/buzzer (+) | GPIO0 | Điều khiển bằng LEDC |
| Còi/buzzer (-) | GND | Còi 5V nên dùng transistor |
| Nút nhấn | GPIO9 ↔ GND | Pull-up nội bộ, active-low |

Lưu ý:

- GPIO9 là chân strapping (BOOT). Giữ nút khi reset có thể đưa chip vào chế độ nạp firmware; chỉ bấm khi thiết bị đang chạy bình thường.
- Module MPU6050 giá rẻ thường đã có pull-up trên board. Nếu dây dài hoặc I2C không ổn định, rút ngắn dây hoặc giảm `i2c_freq` xuống 50 kHz trong `components/mpu_manager/include/mpu_config.h`.
- Nếu còi không kêu hoặc kêu yếu, kiểm tra còi cần áp/dòng bao nhiêu và dùng transistor + nguồn ngoài nếu cần.

## Cấu trúc dự án

```
Fall_Detection/
├── src/
│   ├── main.c                  # Khởi tạo, task còi, task nút, task trạng thái mạng
│   └── CMakeLists.txt
├── components/
│   ├── mpu6050/                # Driver MPU6050
│   ├── fall_detection/         # Máy trạng thái phát hiện ngã
│   ├── mpu_manager/            # Task đọc cảm biến + mpu_config.h
│   ├── button/                 # Nút nhấn + ngắt GPIO
│   ├── buzzer/                 # Còi LEDC, mute/stop tức thời
│   ├── ble/                    # NimBLE, NUS GATT, provisioning WiFi
│   └── wifi/                   # WiFi STA, lưu credentials vào NVS
├── extras/                     # Module tùy chọn, không build mặc định
│   ├── cli/
│   ├── fs/
│   ├── max30102/
│   ├── classic_bt/
│   └── wifi_cli/
├── docs/
│   ├── flow_project.md
│   ├── flow_form_source.txt
│   └── command.txt
├── partitions/
│   └── partitions_large.csv    # nvs 0x6000, phy 0x1000, factory 1500K
├── archive/                    # File cũ/backup, không build
├── platformio.ini
├── sdkconfig.esp32-c3
└── README.md
```

## Cách hoạt động

### Phát hiện té ngã

Máy trạng thái trong `components/fall_detection/fall_detection.c`:

```
NORMAL -> FREE_FALL -> IMPACT -> TILT -> FALL
```

- Gia tốc được lọc thông thấp để ước lượng vector trọng lực.
- Hướng cơ thể trước khi ngã được ghi lại để tính góc nghiêng tương đối.
- Xác nhận ngã khi người dùng nằm nghiêng và nằm yên trong khoảng thời gian cấu hình.
- Sau khi báo ngã có cooldown 10 giây để tránh báo lặp.

Thông số mặc định (`fall_get_default_config`):

| Tham số | Giá trị |
|---|---|
| Ngưỡng free-fall | < 0.60 g |
| Thời gian free-fall tối thiểu | 100 ms |
| Ngưỡng va chạm | > 2.50 g |
| Cửa sổ va chạm | 800 ms |
| Ngưỡng nghiêng | ≥ 45° so với hướng trước ngã |
| Cửa sổ nghiêng | 2500 ms |
| Thời gian xác nhận nằm yên | 800 ms |
| Cooldown báo ngã | 10 s |

### Cấu hình MPU6050

`components/mpu_manager/include/mpu_config.h`:

- Accel ±8 g, gyro ±500 °/s
- DLPF 94 Hz, sample rate nội bộ 200 Hz
- Đọc mỗi 10 ms (100 Hz)
- I2C 100 kHz, SDA GPIO5, SCL GPIO4
- Tự hiệu chuẩn khi khởi động: tắt

### Nút nhấn và còi

| Thao tác | Kết quả |
|---|---|
| Bấm khi còi đang kêu | Tắt còi ngay, nghỉ 2 giây |
| Giữ nút 5 giây (khi không báo động) | Bật còi SOS thủ công |
| Phát hiện ngã | Bật còi SOS, gửi BLE `FALL` + `I:` + `T:` |
| Bấm nút để tắt | Gửi BLE `CLEAR` |

Còi dùng mẫu SOS (3 ngắn, 3 dài, 3 ngắn), lặp lại theo chu kỳ. Hàm `pwm_buzzer_mute()` đảm bảo còi không thể kêu lại do race condition sau khi đã bấm tắt.

### WiFi

- Chế độ STA, không phát AP.
- Khi khởi động, đọc SSID/mật khẩu từ NVS và tự kết nối nếu có.
- Khi mất kết nối, thử lại tối đa 3 lần.
- Credentials chỉ được ghi vào NVS sau khi nhận được IP.
- Xoá cấu hình bằng lệnh BLE `CLR`.
- Hiện chưa gửi dữ liệu lên server/cloud.

### BLE

- Tên thiết bị: `FallDetector`
- Dịch vụ: Nordic UART Service (NUS)
- MTU ưu tiên: 247

| Vai trò | UUID | Thuộc tính |
|---|---|---|
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Primary |
| RX (điện thoại ghi) | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | Write, Write Without Response |
| TX (điện thoại nhận) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | Notify, Read |

TX còn cho phép đọc để lấy trạng thái gần nhất.

Thông báo thiết bị gửi (mỗi message ≤ 20 byte):

| Message | Ý nghĩa |
|---|---|
| `READY` | Điện thoại đã đăng ký notify |
| `FALL` | Phát hiện ngã |
| `I:<x.xx>g` | Gia tốc va chạm lớn nhất |
| `T:<tilt>` | Góc nghiêng cuối cùng (độ) |
| `CLEAR` | Còi đã được tắt |
| `WIFI:UP` | WiFi đã kết nối |
| `IP:<addr>` | Địa chỉ IP nhận được |
| `WIFI:DOWN` | Mất kết nối WiFi |
| `WIFI:FAIL` | Kết nối WiFi thất bại |
| `CFG:OK` | Đã nhận cấu hình WiFi |
| `CFG:ERR` | Lệnh không hợp lệ |
| `CFG:BUSY` | Hàng đợi cấu hình đang bận |
| `CFG:CLR` | Đã xoá cấu hình WiFi |

## Cấu hình WiFi qua BLE

Dùng app BLE bất kỳ hỗ trợ NUS (nRF Connect, Serial Bluetooth Terminal, ...):

1. Quét và kết nối tới `FallDetector`.
2. Bật notify cho characteristic TX (`...0003...`).
3. Ghi vào characteristic RX (`...0002...`) chuỗi:

```
WIFI:<ssid>,<mật_khẩu>
```

Ví dụ:

```
WIFI:NhaToi,12345678
```

4. Thiết bị trả `CFG:OK`, sau đó là `WIFI:UP` và `IP:...` nếu thành công, hoặc `WIFI:FAIL` nếu thất bại.
5. Xoá mạng đã lưu: ghi `CLR` (hoặc `WIFI:CLR`).

Lưu ý: firmware xử lý mỗi lần ghi BLE là một lệnh hoàn chỉnh. Nếu SSID + mật khẩu dài hơn dung lượng một gói (MTU − 3 byte), hãy tăng MTU trong app hoặc dùng mật khẩu/SSID ngắn hơn. Độ dài tối đa: SSID 32 ký tự, mật khẩu 64 ký tự.

## Build và nạp firmware

Mở project bằng VS Code + PlatformIO, chọn env `esp32-c3`, dùng các nút Build / Upload / Monitor. Hoặc dùng terminal:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

Chọn cổng thủ công trên Windows:

```bash
pio run -t upload --upload-port COM5
pio device monitor --port COM5 -b 115200
```

Nếu gặp lỗi CMake kiểu `Couldn't find target config target-...json`, xoá build cache rồi build lại:

```bash
pio run -t fullclean
pio run
```

Firmware hiện khoảng 1.03 MiB, chiếm gần 70% phân vùng app 1500 KB. Nếu bật thêm tính năng lớn, kiểm tra lại kích thước phân vùng.

## Kiểm tra nhanh sau khi nạp

1. Mở serial monitor 115200: thấy log khởi động, 2 tiếng bíp, `[OK] MPU initialized`, `[OK] Fall detection started`, `[OK] WiFi station started`, `[OK] BLE ready`.
2. Dùng điện thoại quét BLE, thấy `FallDetector`, kết nối và bật notify TX: nhận `READY`.
3. Ghi `WIFI:ssid,pass` vào RX: nhận `CFG:OK`, sau đó `WIFI:UP` + `IP:...`.
4. Giữ nút 5 giây: còi SOS kêu; bấm nút: còi tắt ngay, nhận `CLEAR`.
5. Lắc/ngã thử thiết bị: xem log `[FALL] Free fall`, `Impact`, `Tilt`, `DETECTED` và BLE nhận `FALL`, `I:...`, `T:...`.

## Xử lý sự cố

| Hiện tượng | Kiểm tra |
|---|---|
| `[ERROR] MPU init failed!` | Đấu dây SDA/SCL, nguồn 3V3, địa chỉ 0x68, dây quá dài, pull-up |
| I2C lỗi ngẫu nhiên | Giảm `i2c_freq` xuống 50000, rút ngắn dây, kiểm tra GND chung |
| Không thấy `FallDetector` trên BLE | Kiểm tra log BLE init, reset board, tắt/bật Bluetooth điện thoại |
| Kết nối BLE nhưng không nhận notify | Phải bật notify/indicate cho characteristic TX |
| Ghi `WIFI:...` nhưng không có phản hồi | Kiểm tra đúng characteristic RX, đúng định dạng có dấu phẩy |
| `WIFI:FAIL` | Sai SSID/mật khẩu, mạng 5 GHz (ESP32-C3 chỉ 2.4 GHz), router chặn |
| Còi kêu yếu/không kêu | Kiểm tra còi, transistor, nguồn; GPIO0 dùng LEDC |
| Nút không tác dụng | Nối GPIO9 với GND khi bấm, không giữ nút lúc reset |
| Build lỗi target config JSON | `pio run -t fullclean` rồi build lại |

## Giới hạn hiện tại

- Chưa gửi dữ liệu té ngã lên server (chưa có HTTP/MQTT).
- Chưa có chế độ AP để cấu hình WiFi; chỉ cấu hình qua BLE.
- Provisioning WiFi nhận một lệnh trọn vẹn mỗi lần ghi BLE (xem lưu ý về MTU).
- Credentials WiFi lưu dạng plaintext trong NVS.
- Đây là thiết bị hỗ trợ cảnh báo, không phải thiết bị y tế.

## Tài liệu tham khảo

- `docs/flow_project.md`: ghi chú thiết kế và lịch sử thay đổi chi tiết.
- ESP-IDF: https://docs.espressif.com/projects/esp-idf/
- PlatformIO: https://docs.platformio.org/
- NimBLE: https://mynewt.apache.org/latest/network/index.html
