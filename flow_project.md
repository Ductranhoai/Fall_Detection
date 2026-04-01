# PROJECT FALL DETECTION

## Organizational chart

Sơ đồ tổ chức

Fall_Detection_ESP32/
│
├── components/                     # Các thành phần độc lập
│   ├── cli/                        # Giao diện dòng lệnh
│   │   ├── cli.c                   # Khởi tạo và quản lý CLI
│   │   ├── cli.h                   # Header file
│   │   ├── cli_cmd_fs.c            # Lệnh quản lý file
│   │   ├── cli_cmd_gpio.c          # Lệnh điều khiển GPIO
│   │   ├── cli_cmd_i2c.c           # Lệnh I2C scan/read/write
│   │   ├── cli_cmd_log.c           # Lệnh xem log
│   │   ├── cli_cmd_mem.c           # Lệnh dump memory
│   │   ├── cli_cmd_mpu.c           # Lệnh điều khiển MPU
│   │   ├── cli_cmd_system.c        # Lệnh hệ thống (reboot, free)
│   │   ├── cli_cmd_fall.c          # Lệnh phát hiện té ngã
│   │   └── CMakeLists.txt          # Build configuration
│   │
│   ├── mpu/                        # Xử lý cảm biến MPU6050
│   │   ├── mpu6050.c               # Driver giao tiếp MPU6050
│   │   ├── mpu6050.h               # Header driver
│   │   ├── mpu_manager.c           # Quản lý cấp cao
│   │   ├── mpu_manager.h           # Header manager
│   │   ├── fall_detection.c        # Logic phát hiện té ngã
│   │   ├── fall_detection.h        # Header fall detection
│   │   └── CMakeLists.txt          # Build configuration
│   │
│   └── fs/                         # Hệ thống file
│       ├── fs.c                    # FATFS implementation
│       ├── fs.h                    # Header
│       └── CMakeLists.txt          # Build configuration
│
├── src/                            # Source code chính
│   ├── main.c                      # Hàm app_main()
│   └── CMakeLists.txt              # Build configuration
│
├── CMakeLists.txt                  # Project root CMake
├── sdkconfig                       # ESP-IDF configuration
└── platformio.ini                  # PlatformIO configuration


3. COMPONENT CLI - GIAO DIỆN DÒNG LỆNH
3.1. Core CLI (cli.c/h)
3.1.1. Mô tả
Đây là thành phần cốt lõi của CLI, cung cấp khả năng nhập lệnh từ UART và thực thi các lệnh đã đăng ký.

3.1.2. Code chi tiết - cli.c
```c
#include "cli.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
```
Giải thích các thư viện:

esp_console.h: Cung cấp API để đăng ký và thực thi lệnh
linenoise/linenoise.h: Thư viện xử lý dòng lệnh (history, auto-complete)
esp_vfs_dev.h: Virtual File System cho UART
driver/uart.h: Driver UART để giao tiếp với console
```c
c
void cli_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
```
Giải thích:

Cấu hình UART0 với baud rate 115200, 8 data bits, no parity, 1 stop bit
uart_driver_install: Cài đặt driver với buffer 256 bytes
UART_NUM_0 là UART mặc định (GPIO1=TX, GPIO3=RX)

```c
c
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
#pragma GCC diagnostic pop
```

Giải thích:

Kết nối UART với Virtual File System

#pragma để bỏ qua warning về hàm deprecated (vẫn dùng được)

c
    esp_console_config_t console_config = {
        .max_cmdline_args = 8,
        .max_cmdline_length = 256,
    };

    esp_console_init(&console_config);

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(50);
}
Giải thích:

max_cmdline_args: Tối đa 8 arguments cho mỗi lệnh

max_cmdline_length: Độ dài tối đa của dòng lệnh (256 ký tự)

linenoiseSetMultiLine(1): Cho phép nhập lệnh nhiều dòng

linenoiseHistorySetMaxLen(50): Lưu 50 lệnh gần nhất

c
void cli_start(void)
{
    char *line;

    while (true)
    {
        line = linenoise("esp32>> ");
        if (!line)
            continue;

        int ret;
        esp_console_run(line, &ret);

        linenoiseHistoryAdd(line);
        linenoiseFree(line);
    }
}
Giải thích:

Vòng lặp vô hạn để nhận lệnh từ user

linenoise("esp32>> "): Hiển thị prompt và chờ nhập

esp_console_run(line, &ret): Parse và thực thi lệnh

Lưu lệnh vào history và giải phóng bộ nhớ

c
static void cli_task(void *arg)
{
    cli_start();
}

void cli_init_all(void)
{
    // 1. init console + UART
    cli_init();

    // 2. register command
    cli_register_fs();
    cli_register_mem();
    cli_register_i2c();
    cli_register_gpio();
    cli_register_system();
    cli_register_mpu();
    cli_register_fall();

#ifdef CONFIG_CLI_ENABLE_LOG
    cli_register_log();
#endif

    // 3. start CLI task (KHÔNG block main)
    xTaskCreate(cli_task, "cli", 8192, NULL, 5, NULL);
}
Giải thích:

cli_register_*(): Đăng ký các nhóm lệnh

xTaskCreate: Tạo task riêng cho CLI với stack 8192 bytes, priority 5

Task này chạy độc lập, không block main task

3.1.3. cli.h - Header file
c
#pragma once

void cli_init(void);
void cli_start(void);
void cli_init_all(void);

//register group
void cli_register_system(void);
void cli_register_fs(void);
void cli_register_mem(void);
void cli_register_i2c(void);
void cli_register_gpio(void);
void cli_register_log(void);
void cli_register_mpu(void);
void cli_register_fall(void);
Giải thích:

#pragma once: Đảm bảo header chỉ được include một lần

Các hàm đăng ký được khai báo để các file khác có thể sử dụng

3.2. Filesystem Commands (cli_cmd_fs.c)
3.2.1. Mô tả
Cung cấp các lệnh để thao tác với hệ thống file: ls, pwd, cd, touch, cat, mkdir, rm.

3.2.2. Code chi tiết
c
#include "esp_console.h"
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#define BASE_PATH "/fatfs"
#define MAX_PATH_LEN 128

static char cwd[128] = "/fatfs";
Giải thích:

BASE_PATH: Đường dẫn gốc của hệ thống file

cwd: Current Working Directory, lưu đường dẫn hiện tại

MAX_PATH_LEN: Độ dài tối đa của đường dẫn

c
static int build_path(char *out, size_t size, const char *cwd, const char *name)
{
    int needed = snprintf(out, size, "%s/%s", cwd, name);

    if (needed < 0 || needed >= size)
    {
        printf("Path too long\n");
        return -1;
    }

    return 0;
}
Giải thích:

Xây dựng đường dẫn tuyệt đối từ cwd và tên file/folder

Kiểm tra tràn buffer để tránh lỗi bảo mật

c
static int cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : cwd;

    DIR *dir = opendir(path);
    if (!dir)
    {
        printf("Cannot open %s\n", path);
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
    return 0;
}
Giải thích:

opendir(): Mở thư mục

readdir(): Đọc từng entry trong thư mục

In tên file/folder ra console

c
static int cmd_cd(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    char newpath[MAX_PATH_LEN];

    if (argv[1][0] == '/')
    {
        strncpy(newpath, argv[1], sizeof(newpath));
    }
    else
    {
        if (build_path(newpath, sizeof(newpath), cwd, argv[1]) != 0)
        {
            return 1;
        }
    }

    DIR *dir = opendir(newpath);
    if (!dir)
    {
        printf("No such dir\n");
        return 1;
    }

    closedir(dir);
    strncpy(cwd, newpath, sizeof(cwd) - 1);
    cwd[sizeof(cwd) - 1] = '\0';

    return 0;
}
Giải thích:

Nếu path bắt đầu bằng '/': đường dẫn tuyệt đối

Nếu không: đường dẫn tương đối so với cwd

Kiểm tra thư mục tồn tại trước khi đổi

Cập nhật cwd nếu thành công

c
static int cmd_touch(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    char path[128];
    if (build_path(path, sizeof(path), cwd, argv[1]) != 0)
    {
        return 1;
    }

    FILE *f = fopen(path, "w");
    if (!f)
    {
        printf("Create failed\n");
        return 1;
    }

    fclose(f);
    return 0;
}
Giải thích:

fopen(path, "w"): Mở file ở chế độ write, tạo mới nếu chưa tồn tại

Đóng file ngay sau khi tạo

c
static int cmd_cat(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    char path[MAX_PATH_LEN];

    if (build_path(path, sizeof(path), cwd, argv[1]) != 0)
    {
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (!f)
    {
        printf("Open failed\n");
        return 1;
    }

    char buf[128];
    while (fgets(buf, sizeof(buf), f))
    {
        printf("%s", buf);
    }

    fclose(f);
    return 0;
}
Giải thích:

fopen(path, "r"): Mở file ở chế độ read

fgets(): Đọc từng dòng và in ra console

c
static int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    char path[128];
    if (build_path(path, sizeof(path), cwd, argv[1]) != 0)
    {
        return 1;
    }

    mkdir(path, 0777);
    return 0;
}
Giải thích:

mkdir(path, 0777): Tạo thư mục với permission 0777 (read/write/execute)

c
static int cmd_rm(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    char path[128];
    if (build_path(path, sizeof(path), cwd, argv[1]) != 0)
    {
        return 1;
    }

    unlink(path);
    return 0;
}
Giải thích:

unlink(path): Xóa file (xóa thư mục không hỗ trợ)

c
void cli_register_fs(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ls",
        .func = cmd_ls,
    });

    // ... các lệnh khác
}
Giải thích:

esp_console_cmd_register: Đăng ký lệnh với console

Mỗi lệnh có tên và hàm xử lý tương ứng

3.3. GPIO Commands (cli_cmd_gpio.c)
3.3.1. Mô tả
Cung cấp lệnh điều khiển GPIO: set output level, read input level.

3.3.2. Code chi tiết
c
#include "esp_console.h"
#include "driver/gpio.h"
#include <stdlib.h>
#include <stdio.h>

static int cmd_gpio_set(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: gpio_set <pin> <0|1>\n");
        return 1;
    }

    int pin = atoi(argv[1]);
    int level = atoi(argv[2]);

    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);

    printf("GPIO %d = %d\n", pin, level);
    return 0;
}
Giải thích:

atoi(): Chuyển string sang integer

gpio_set_direction(): Cấu hình GPIO là output

gpio_set_level(): Set mức logic 0 hoặc 1

c
static int cmd_gpio_get(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: gpio_get <pin>\n");
        return 1;
    }

    int pin = atoi(argv[1]);
    gpio_set_direction(pin, GPIO_MODE_INPUT);

    int level = gpio_get_level(pin);
    printf("GPIO %d = %d\n", pin, level);
    return 0;
}
Giải thích:

Cấu hình GPIO là input

gpio_get_level(): Đọc mức logic (0 hoặc 1)

3.4. I2C Commands (cli_cmd_i2c.c)
3.4.1. Mô tả
Cung cấp lệnh để scan I2C bus, đọc và ghi thanh ghi.

3.4.2. Code chi tiết
c
#include "esp_console.h"
#include "driver/i2c.h"
#include <stdlib.h>
#include <stdio.h>

#define I2C_MASTER_NUM I2C_NUM_0

static int cmd_i2c_scan(int argc, char **argv)
{
    for (int addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        if (i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 50 / portTICK_PERIOD_MS) == ESP_OK) {
            printf("Found: 0x%02X\n", addr);
        }

        i2c_cmd_link_delete(cmd);
    }
    return 0;
}
Giải thích:

Quét tất cả địa chỉ I2C từ 1 đến 126

Tạo command: START → Write address (write mode) → STOP

Nếu device ACK (ESP_OK) → in ra địa chỉ

Timeout 50 ticks (~50ms)

c
static int cmd_i2c_read(int argc, char **argv)
{
    uint8_t dev = strtol(argv[1], NULL, 0);
    uint8_t reg = strtol(argv[2], NULL, 0);
    uint8_t data;

    i2c_master_write_read_device(I2C_MASTER_NUM, dev, &reg, 1, &data, 1, 1000 / portTICK_PERIOD_MS);

    printf("0x%02X = 0x%02X\n", reg, data);
    return 0;
}
Giải thích:

strtol(): Chuyển string sang số với base tự động (0: auto-detect)

i2c_master_write_read_device(): Write register address, read 1 byte data

Timeout 1000 ticks (~1 giây)

c
static int cmd_i2c_write(int argc, char **argv)
{
    uint8_t dev = strtol(argv[1], NULL, 0);
    uint8_t reg = strtol(argv[2], NULL, 0);
    uint8_t data = strtol(argv[3], NULL, 0);

    uint8_t buf[2] = {reg, data};

    i2c_master_write_to_device(I2C_MASTER_NUM, dev, buf, 2, 1000 / portTICK_PERIOD_MS);

    printf("Write OK\n");
    return 0;
}
Giải thích:

Tạo buffer: [register, data]

i2c_master_write_to_device(): Ghi 2 bytes vào device

3.5. Log Commands (cli_cmd_log.c)
3.5.1. Mô tả
Hook ESP_LOG để lưu logs vào buffer và cung cấp lệnh dmesg để xem logs.

3.5.2. Code chi tiết
c
#include "esp_console.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_SIZE 4096

static char log_buffer[LOG_BUFFER_SIZE];
static int log_index = 0;
static vprintf_like_t original_vprintf = NULL;
Giải thích:

log_buffer: Buffer vòng để lưu logs

log_index: Vị trí ghi hiện tại

original_vprintf: Lưu hàm printf gốc để gọi lại

c
static int log_vprintf(const char *fmt, va_list args)
{
    char temp[256];
    int len = vsnprintf(temp, sizeof(temp), fmt, args);

    // print ra UART như bình thường
    if (original_vprintf) {
        original_vprintf(fmt, args);
    }

    // lưu vào buffer
    if (len > 0) {
        if (log_index + len >= LOG_BUFFER_SIZE) {
            log_index = 0; // overwrite kiểu vòng
        }
        memcpy(&log_buffer[log_index], temp, len);
        log_index += len;
    }

    return len;
}
Giải thích:

Hook function để thay thế printf

vsnprintf(): Format string vào buffer tạm

Gọi original printf để in ra UART

Lưu vào buffer vòng (circular buffer)

c
static int cmd_dmesg(int argc, char **argv)
{
    printf("---- dmesg ----\n");

    fwrite(log_buffer, 1, log_index, stdout);

    return 0;
}
Giải thích:

In toàn bộ buffer ra console

c
void cli_register_log(void)
{
    // hook log
    original_vprintf = esp_log_set_vprintf(log_vprintf);

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "dmesg",
        .help = "Show log buffer",
        .func = cmd_dmesg,
    });
}
Giải thích:

esp_log_set_vprintf(): Thay thế hàm in log mặc định

3.6. Memory Commands (cli_cmd_mem.c)
3.6.1. Mô tả
Cung cấp lệnh hexdump để xem nội dung memory.

3.6.2. Code chi tiết
c
#include "esp_console.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

static int cmd_hexdump(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: hexdump <addr> <len>\n");
        return 1;
    }

    uint8_t *addr = (uint8_t *)strtol(argv[1], NULL, 0);
    int len = atoi(argv[2]);

    if (len <= 0) {
        printf("Invalid length\n");
        return 1;
    }

    for (int i = 0; i < len; i += 16) {
        printf("%08x  ", (unsigned int)(addr + i));

        // hex part
        for (int j = 0; j < 16; j++) {
            if (i + j < len)
                printf("%02x ", addr[i + j]);
            else
                printf("   ");
        }

        printf(" ");

        // ascii part
        for (int j = 0; j < 16 && (i + j < len); j++) {
            uint8_t c = addr[i + j];
            printf("%c", isprint(c) ? c : '.');
        }

        printf("\n");
    }

    return 0;
}
Giải thích:

strtol(): Chuyển địa chỉ từ string sang số

Mỗi dòng 16 bytes, hiển thị cả hex và ascii

isprint(): Kiểm tra ký tự in được

3.7. MPU Commands (cli_cmd_mpu.c)
3.7.1. Mô tả
Cung cấp các lệnh điều khiển MPU: đọc dữ liệu, calibration, monitoring.

3.7.2. Code chi tiết (phần quan trọng)
c
#include "esp_console.h"
#include "mpu_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "linenoise/linenoise.h"

static volatile bool continuous_read_active = false;
static TaskHandle_t continuous_read_task_handle = NULL;
static volatile bool stop_requested = false;
Giải thích:

continuous_read_active: Flag đánh dấu đang ở chế độ đọc liên tục

stop_requested: Flag yêu cầu dừng (Ctrl+C)

continuous_read_task_handle: Task handle để quản lý

c
static void handle_ctrl_c(void)
{
    stop_requested = true;
    printf("\n\n⚠️  Ctrl+C detected. Stopping continuous read...\n");
}
Giải thích:

Callback khi nhấn Ctrl+C

Set flag để task dừng

c
static void continuous_read_task_func(void *arg)
{
    mpu6050_data_t data;
    stop_requested = false;
    
    printf("\n========================================\n");
    printf("   Continuous MPU Read Mode (2s interval)\n");
    printf("   Press Ctrl+C to stop\n");
    printf("========================================\n\n");
    
    while (continuous_read_active && !stop_requested) {
        if (mpu_manager_get_data(&data) == ESP_OK) {
            // In dữ liệu
            printf("\n--- Update ---\n");
            printf("Accel (g): X=%7.3f  Y=%7.3f  Z=%7.3f\n", 
                   data.accel_x, data.accel_y, data.accel_z);
            printf("Angles: Pitch=%7.2f°  Roll=%7.2f°\n", data.pitch, data.roll);
            printf("Temp: %.2f°C\n", data.temperature);
            printf("--- Next update in 2 seconds ---\n");
        }
        
        // Delay 2s nhưng kiểm tra flag mỗi 100ms
        for (int i = 0; i < 20 && continuous_read_active && !stop_requested; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    printf("\n✅ Continuous read stopped.\n");
    continuous_read_active = false;
    continuous_read_task_handle = NULL;
    vTaskDelete(NULL);
}
Giải thích:

Vòng lặp chính đọc và in dữ liệu mỗi 2 giây

Kiểm tra flag stop_requested để thoát

Sử dụng vTaskDelay nhỏ (100ms) để phản hồi Ctrl+C nhanh

3.8. System Commands (cli_cmd_system.c)
3.8.1. Mô tả
Cung cấp lệnh hệ thống: reboot, free, tasks.

3.8.2. Code chi tiết
c
#include "esp_console.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <stdio.h>

static int cmd_reboot(int argc, char **argv)
{
    printf("Rebooting...\n");
    esp_restart();
    return 0;
}
Giải thích:

esp_restart(): Khởi động lại ESP32

c
static int cmd_free(int argc, char **argv)
{
    printf("Free heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    return 0;
}
Giải thích:

heap_caps_get_free_size(): Lấy dung lượng heap trống

c
static int cmd_tasks(int argc, char **argv)
{
    printf("Task list:\n");
    vTaskList(NULL); 
    return 0;
}
Giải thích:

vTaskList(): In danh sách tasks (cần cấu hình trong menuconfig)

3.9. Fall Detection Commands (cli_cmd_fall.c)
3.9.1. Mô tả
Cung cấp lệnh giám sát trạng thái fall detection.

3.9.2. Code chi tiết
c
#include "esp_console.h"
#include "fall_detection.h"
#include <stdio.h>
#include <string.h>

static int cmd_fall_status(int argc, char **argv)
{
    fall_result_t result = fall_detection_get_result();
    
    printf("\n========================================\n");
    printf("      Fall Detection Status            \n");
    printf("========================================\n");
    printf("\nCurrent State: %s\n", fall_state_to_string(result.state));
    printf("Fall Detected: %s\n", result.fall_detected ? "YES" : "NO");
    
    if (result.fall_detected) {
        printf("\n--- Fall Details ---\n");
        printf("  Min Acceleration: %.2fg\n", result.min_accel);
        printf("  Max Acceleration: %.2fg\n", result.max_accel);
        printf("  Final Tilt: %.1f°\n", result.final_tilt);
        printf("  Reason: %s\n", result.detection_reason);
    }
    
    printf("\n========================================\n");
    return 0;
}
Giải thích:

Lấy kết quả fall detection hiện tại

Hiển thị state, flag và chi tiết nếu có fall

4. COMPONENT MPU - XỬ LÝ CẢM BIẾN
4.1. MPU6050 Driver (mpu6050.c/h)
4.1.1. Mô tả
Driver giao tiếp trực tiếp với MPU6050 qua I2C.

4.1.2. Cấu trúc dữ liệu
c
// Cấu hình MPU6050
typedef struct {
    i2c_port_t i2c_port;              // I2C port (I2C_NUM_0 hoặc I2C_NUM_1)
    uint32_t i2c_freq_hz;              // Tần số I2C (thường 400kHz)
    uint8_t sda_pin;                   // GPIO cho SDA
    uint8_t scl_pin;                   // GPIO cho SCL
    mpu_accel_range_t accel_range;     // Dải đo gia tốc
    mpu_gyro_range_t gyro_range;       // Dải đo gyro
    mpu_dlpf_bandwidth_t dlpf_bandwidth; // Bộ lọc thông thấp
    uint32_t sample_rate_hz;           // Tần số lấy mẫu nội bộ
    bool enable_fifo;                  // Bật FIFO buffer
    uint16_t fifo_buffer_size;         // Kích thước FIFO buffer
} mpu6050_config_t;

// Dữ liệu từ MPU6050
typedef struct {
    // Raw data
    int16_t ax, ay, az;                // Gia tốc raw (16-bit)
    int16_t gx, gy, gz;                // Gyro raw (16-bit)
    
    // Converted data
    float accel_x, accel_y, accel_z;   // Gia tốc (g)
    float gyro_x, gyro_y, gyro_z;      // Gyro (deg/s)
    float pitch, roll;                 // Góc nghiêng (độ)
    float temperature;                 // Nhiệt độ (°C)
    
    // Timestamp
    uint32_t timestamp_ms;              // Thời gian đọc (ms)
} mpu6050_data_t;
4.1.3. Các hàm chính
c
esp_err_t mpu6050_init(const mpu6050_config_t *config);
Flow xử lý:

Cấu hình I2C pins và tần số

Cài đặt I2C driver

Wake up MPU (ghi 0 vào PWR_MGMT_1)

Kiểm tra WHO_AM_I (phải là 0x68)

Cấu hình accelerometer range

Cấu hình gyroscope range

Cấu hình DLPF (Digital Low Pass Filter)

Cấu hình sample rate

c
esp_err_t mpu6050_read(mpu6050_data_t *data);
Flow xử lý:

Đọc 14 bytes từ thanh ghi ACCEL_XOUT_H

Parse 6 bytes accel (2 bytes mỗi trục)

Parse 2 bytes temperature

Parse 6 bytes gyro (2 bytes mỗi trục)

Chuyển đổi sang đơn vị vật lý

Áp dụng bias compensation (nếu có)

Công thức chuyển đổi:

c
// Accelerometer (LSB to g)
accel_x = ax / accel_scale;  // accel_scale = 16384 cho ±2g

// Gyroscope (LSB to deg/s)
gyro_x = gx / gyro_scale;    // gyro_scale = 131 cho ±250°/s

// Temperature (LSB to °C)
temperature = temp_raw / 340.0 + 36.53;
c
esp_err_t mpu6050_calc_angles(mpu6050_data_t *data);
Công thức:

c
// Pitch: góc lên/xuống
pitch = atan2(-accel_x, sqrt(accel_y² + accel_z²)) * 180/π;

// Roll: góc nghiêng trái/phải
roll = atan2(accel_y, accel_z) * 180/π;
4.2. MPU Manager (mpu_manager.c/h)
4.2.1. Mô tả
Lớp quản lý cấp cao cho MPU, cung cấp:

Khởi tạo và cấu hình

Đọc dữ liệu tuần hoàn

Callback khi có dữ liệu mới

4.2.2. Cấu trúc
c
typedef struct {
    uint8_t sda_pin;              // GPIO SDA (default: 21)
    uint8_t scl_pin;              // GPIO SCL (default: 22)
    i2c_port_t i2c_port;          // I2C port (default: I2C_NUM_0)
    uint32_t i2c_freq;            // I2C frequency (default: 400000)
    mpu_accel_range_t accel_range; // ±2g, ±4g, ±8g, ±16g
    mpu_gyro_range_t gyro_range;   // ±250, ±500, ±1000, ±2000 °/s
    mpu_dlpf_bandwidth_t dlpf_bandwidth; // Bộ lọc thông thấp
    uint32_t sample_rate_hz;      // Sample rate (default: 100Hz)
    uint32_t read_interval_ms;    // Read interval (default: 100ms)
    bool enable_fifo;              // Enable FIFO buffer
    bool enable_calibration;       // Auto-calibration on startup
} mpu_config_t;
4.2.3. Các hàm chính
c
esp_err_t mpu_manager_init(const mpu_config_t *config);
Flow:

Copy cấu hình

Chuyển đổi sang mpu6050_config_t

Gọi mpu6050_init()

Calibrate nếu enable_calibration = true

Khởi tạo fall detection

c
void mpu_manager_start_monitoring(mpu_data_callback_t callback);
Flow:

Lưu callback

Tạo task riêng để đọc sensor với interval read_interval_ms

Trong task: đọc data → xử lý → gọi callback

4.3. Fall Detection (fall_detection.c/h)
4.3.1. Mô tả
Phát hiện té ngã sử dụng state machine với 5 trạng thái.

4.3.2. State Machine
c
typedef enum {
    FALL_STATE_NORMAL,      // Trạng thái bình thường
    FALL_STATE_FREE_FALL,   // Đang rơi tự do
    FALL_STATE_IMPACT,      // Va chạm
    FALL_STATE_TILT,        // Nghiêng sau khi ngã
    FALL_STATE_FALL         // Xác nhận té ngã
} fall_state_t;
4.3.3. Cấu trúc kết quả
c
typedef struct {
    fall_state_t state;              // State hiện tại
    bool fall_detected;              // Đã phát hiện té ngã?
    uint32_t fall_timestamp;         // Thời gian té ngã (ms)
    float max_accel;                 // Gia tốc lớn nhất khi impact
    float min_accel;                 // Gia tốc nhỏ nhất khi free fall
    float final_tilt;                // Góc nghiêng cuối cùng
    char detection_reason[64];       // Lý do phát hiện
} fall_result_t;
4.3.4. Thuật toán phát hiện
c
esp_err_t fall_detection_process(mpu6050_data_t *data)
{
    float total_accel = sqrt(ax² + ay² + az²);
    uint32_t now = esp_timer_get_time() / 1000;
    
    switch (s_current_state) {
        case FALL_STATE_NORMAL:
            if (total_accel < FREE_FALL_THRESHOLD) {
                s_current_state = FALL_STATE_FREE_FALL;
                s_free_fall_start_time = now;
            }
            break;
            
        case FALL_STATE_FREE_FALL:
            if (now - s_free_fall_start_time >= FREE_FALL_MIN_TIME) {
                s_current_state = FALL_STATE_IMPACT;
                s_impact_time = now;
            }
            break;
            
        case FALL_STATE_IMPACT:
            if (total_accel > IMPACT_THRESHOLD) {
                s_current_state = FALL_STATE_TILT;
            }
            break;
            
        case FALL_STATE_TILT:
            if (fabs(pitch) > TILT_THRESHOLD || fabs(roll) > TILT_THRESHOLD) {
                if (now - s_impact_time >= FALL_CONFIRM_TIME) {
                    s_current_state = FALL_STATE_FALL;
                    // Fall confirmed!
                }
            }
            break;
            
        case FALL_STATE_FALL:
            // Cooldown period
            if (now - fall_timestamp > 10000) {
                s_current_state = FALL_STATE_NORMAL;
            }
            break;
    }
}
5. COMPONENT FS - HỆ THỐNG FILE
5.1. Filesystem Implementation (fs.c/h)
5.1.1. Mô tả
Khởi tạo hệ thống file FAT trên flash SPI sử dụng wear levelling.

5.1.2. Code chi tiết
c
#include "fs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "wear_levelling.h"

#define MOUNT_POINT "/fatfs"

static wl_handle_t wl_handle;
Giải thích:

MOUNT_POINT: Đường dẫn mount (root của filesystem)

wl_handle: Handle cho wear levelling (kỹ thuật san bằng hao mòn flash)

c
void fs_init(void)
{
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 5,                    // Số file mở đồng thời tối đa
        .format_if_mount_failed = true,    // Format nếu mount thất bại
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount(
        MOUNT_POINT,
        "storage",                         // Partition label
        &mount_config,
        &wl_handle
    );

    if (ret != ESP_OK) {
        printf("FATFS mount failed: %s\n", esp_err_to_name(ret));
    } else {
        printf("FATFS mounted at %s\n", MOUNT_POINT);
    }
}
Giải thích:

esp_vfs_fat_spiflash_mount(): Mount FATFS trên partition "storage"

max_files = 5: Tối đa 5 file có thể mở đồng thời

format_if_mount_failed = true: Tự động format nếu lỗi (lần đầu chạy)

Partition "storage" được định nghĩa trong partition table

6. MAIN APPLICATION
6.1. main.c
c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "cli.h"
#include "fs.h"
#include "wifi_manager.h"
#include "wifi_cli.h"
#include "mpu_manager.h"
#include "fall_detection.h"

static const char *TAG = "MAIN";
Giải thích:

Include các header cần thiết

Định nghĩa TAG cho logging

c
static void on_mpu_data(mpu6050_data_t *data)
{
    // Process fall detection
    fall_detection_process(data);
    fall_result_t result = fall_detection_get_result();
    
    // Save to file when fall detected
    if (result.fall_detected) {
        FILE *f = fopen("/fatfs/fall_log.txt", "a");
        if (f) {
            fprintf(f, "Time: %lu, Max: %.2fg, Min: %.2fg, Tilt: %.1f, Reason: %s\n",
                    result.fall_timestamp, result.max_accel,
                    result.min_accel, result.final_tilt, 
                    result.detection_reason);
            fclose(f);
        }
        
        ESP_LOGW(TAG, "⚠️ FALL DETECTED! ⚠️");
        ESP_LOGW(TAG, "  Reason: %s", result.detection_reason);
    }
    
    // Periodic logging
    static uint32_t last_log = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_log > 5000) {
        ESP_LOGI(TAG, "State: %s, Pitch: %.1f°, Roll: %.1f°, Z: %.2fg",
                 fall_state_to_string(result.state),
                 data->pitch, data->roll, data->accel_z);
        last_log = now;
    }
}
Giải thích:

Callback được gọi mỗi khi có dữ liệu MPU mới

Xử lý fall detection và ghi log vào file khi phát hiện té ngã

Log định kỳ 5 giây để giám sát

c
void app_main(void)
{
    ESP_LOGI(TAG, "System starting...");
    
    // 1. Initialize WiFi
    wifi_manager_init();
    
    // 2. Initialize filesystem
    fs_init();
    
    // 3. Initialize MPU with default config
    mpu_config_t mpu_config = mpu_get_default_config();
    esp_err_t ret = mpu_manager_init(&mpu_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MPU!");
    } else {
        // Start monitoring with callback
        mpu_manager_start_monitoring(on_mpu_data);
        ESP_LOGI(TAG, "MPU monitoring started");
    }
    
    // 4. Initialize CLI (will create CLI task)
    cli_init_all();
    
    ESP_LOGI(TAG, "System ready! Use CLI commands:");
    ESP_LOGI(TAG, "  mpu_read, mpu_cal, mpu_monitor, fall_status, etc.");
}
Giải thích:

Thứ tự khởi tạo quan trọng: WiFi → FS → MPU → CLI

MPU monitoring bắt đầu sau khi init thành công

CLI tạo task riêng, không block main

7. HƯỚNG DẪN SỬ DỤNG VÀ DEBUG
7.1. Khởi động hệ thống
Kết nối hardware:

ESP32 với máy tính qua USB

MPU6050: VCC→3.3V, GND→GND, SCL→GPIO22, SDA→GPIO21

Mở terminal (baud rate 115200):

bash
screen /dev/ttyUSB0 115200
# hoặc
putty, minicom, etc.
Reset ESP32 và quan sát log:

text
FATFS mounted at /fatfs
MPU6050 initialized!
MPU monitoring started
System ready!
esp32>>
7.2. Các lệnh cơ bản
bash
# Kiểm tra MPU
esp32>> mpu_test

# Đọc dữ liệu một lần
esp32>> mpu_read

# Đọc liên tục (Ctrl+C để dừng)
esp32>> mpu_read -w

# Calibrate (đặt thiết bị trên mặt phẳng)
esp32>> mpu_cal

# Bật monitoring nền
esp32>> mpu_monitor start

# Xem trạng thái fall detection
esp32>> fall_status

# Xem hệ thống file
esp32>> ls
esp32>> cd /fatfs
esp32>> cat fall_log.txt

# Thông tin hệ thống
esp32>> free
esp32>> tasks
7.3. Test fall detection
Chuẩn bị: Đặt thiết bị trên mặt phẳng, chạy mpu_monitor start

Mô phỏng té ngã:

Bước 1: Thả rơi tự do (free fall) từ độ cao ~20cm

Bước 2: Để thiết bị rơi xuống mặt phẳng mềm (impact)

Bước 3: Để thiết bị nghiêng (tilt)

Quan sát console:

text
I (12345) FALL_DETECT: Free fall detected!
I (12445) FALL_DETECT: Impact detected!
W (12700) FALL_DETECT: ⚠️ FALL DETECTED! ⚠️
W (12700) FALL_DETECT:   Reason: Free fall: 0.32g, Impact: 3.45g, Tilt: 52.3°
7.4. Debug và troubleshooting
Vấn đề	Nguyên nhân	Giải pháp
MPU không phát hiện	Kết nối sai, thiếu pull-up	Kiểm tra dây, thêm pull-up 4.7kΩ
Dữ liệu nhiễu	Nguồn không ổn định	Thêm tụ 10µF gần MPU
Fall detection false	Ngưỡng không phù hợp	Điều chỉnh thresholds
Filesystem lỗi	Partition chưa format	Format tự động khi khởi động
