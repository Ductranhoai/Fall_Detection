#include "max30102.h"
#include "esp_log.h"
#include "math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAX30102";

static i2c_port_t i2c_port;
static bool initialized = false;

static float hr = 0;
static float spo2_val = 0;
static bool finger = false;

#define BUF_SIZE 32
static uint32_t ir_buf[BUF_SIZE];
static int idx = 0;
static int filled = 0;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return ret;
}

static esp_err_t read_fifo(uint32_t *red, uint32_t *ir)
{
    uint8_t data[6];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, MAX30102_REG_FIFO_DATA, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_ADDR << 1) | I2C_MASTER_READ, true);

    for (int i = 0; i < 5; i++)
        i2c_master_read_byte(cmd, &data[i], I2C_MASTER_ACK);

    i2c_master_read_byte(cmd, &data[5], I2C_MASTER_NACK);

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) return ret;

    *ir  = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    *red = ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];

    *ir &= 0x3FFFF;
    *red &= 0x3FFFF;

    return ESP_OK;
}

// ===== HEART RATE (simple peak detect) =====
static void calc_hr(void)
{
    if (filled < BUF_SIZE) return;

    uint32_t min = UINT32_MAX, max = 0;

    for (int i = 0; i < BUF_SIZE; i++) {
        if (ir_buf[i] < min) min = ir_buf[i];
        if (ir_buf[i] > max) max = ir_buf[i];
    }

    uint32_t th = min + (max - min) * 0.6;

    int peaks = 0;
    for (int i = 1; i < BUF_SIZE; i++) {
        if (ir_buf[i] > th && ir_buf[i-1] <= th) {
            peaks++;
        }
    }

    if (peaks > 1) {
        hr = peaks * 6.0f; // rough BPM scaling
    }
}

// ===== SPO2 (simple ratio) =====
static float calc_spo2(uint32_t red, uint32_t ir)
{
    static float r_avg = 0, ir_avg = 0;
    static int n = 0;

    r_avg += red;
    ir_avg += ir;
    n++;

    if (n < 50) return 0;

    r_avg /= n;
    ir_avg /= n;

    float ratio = (red / r_avg) / (ir / ir_avg);

    float spo2 = 110 - 25 * ratio;

    if (spo2 > 100) spo2 = 100;
    if (spo2 < 70) spo2 = 70;

    n = 0;
    r_avg = 0;
    ir_avg = 0;

    return spo2;
}

esp_err_t max30102_init(i2c_port_t port, uint8_t sda, uint8_t scl)
{
    i2c_port = port;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    i2c_param_config(port, &conf);
    i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Part ID check...");

    write_reg(MAX30102_REG_MODE_CONFIG, 0x40);
    vTaskDelay(pdMS_TO_TICKS(50));

    write_reg(MAX30102_REG_FIFO_CONFIG, 0x4F);
    write_reg(MAX30102_REG_MODE_CONFIG, 0x03);
    write_reg(MAX30102_REG_SPO2_CONFIG, 0x27);
    write_reg(MAX30102_REG_LED1_PA, 0x24);
    write_reg(MAX30102_REG_LED2_PA, 0x24);

    initialized = true;

    return ESP_OK;
}

esp_err_t max30102_read(max30102_data_t *data)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;

    uint32_t red, ir;
    esp_err_t ret = read_fifo(&red, &ir);
    if (ret != ESP_OK) return ret;

    // ===== FINGER DETECT =====
    finger = (ir > 50000);

    // buffer
    ir_buf[idx] = ir;
    idx = (idx + 1) % BUF_SIZE;
    if (filled < BUF_SIZE) filled++;

    calc_hr();

    float s = calc_spo2(red, ir);
    if (s > 0) spo2_val = s;

    if (data) {
        data->red = red;
        data->ir = ir;
        data->heart_rate = hr;
        data->spo2 = spo2_val;
        data->finger_detected = finger;
    }

    return ESP_OK;
}

bool max30102_is_finger_present(void)
{
    return finger;
}

esp_err_t max30102_get_heart_rate(float *v)
{
    if (!v) return ESP_ERR_INVALID_ARG;
    *v = hr;
    return ESP_OK;
}

esp_err_t max30102_get_spo2(float *v)
{
    if (!v) return ESP_ERR_INVALID_ARG;
    *v = spo2_val;
    return ESP_OK;
}

void max30102_deinit(void)
{
    i2c_driver_delete(i2c_port);
    initialized = false;
}