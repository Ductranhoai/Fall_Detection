/**
 * @file pulseDriver.c
 * @brief Implementation của Pulse Sensor Driver
 *
 * Thuật toán phát hiện nhịp tim dựa trên phát hiện đỉnh (peak detection)
 * với ngưỡng động tự động điều chỉnh
 */

#include "pulseDriver.h"
#include "../../include/config.h"
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PULSE";

// Biến static cho driver
static bool pulse_initialized = false;
static pulse_detector_t pulse_detector;
static moving_average_filter_t filter;

// ADC handle và calibration
static esp_adc_cal_characteristics_t adc_chars;

// Biến thời gian
static uint32_t last_sample_time = 0;
static uint32_t last_update_time = 0;

// Dữ liệu pulse hiện tại
static pulse_data_t current_pulse = {
    .raw_value = 0,
    .heart_rate = 0,
    .is_beat = false,
    .confidence = 0.0f,
    .last_beat_time = 0};

/**
 * @brief Khởi tạo bộ lọc trung bình động
 */
static void filter_init(moving_average_filter_t *f)
{
    f->index = 0;
    f->sum = 0;
    f->count = 0;
    memset(f->buffer, 0, sizeof(f->buffer));
}

/**
 * @brief Thêm giá trị vào bộ lọc và lấy giá trị trung bình
 */
static int filter_add(moving_average_filter_t *f, int value)
{
    // Nếu chưa đầy buffer
    if (f->count < PULSE_BUFFER_SIZE)
    {
        f->buffer[f->index] = value;
        f->sum += value;
        f->count++;
    }
    else
    {
        // Đã đầy, thay thế giá trị cũ nhất
        f->sum -= f->buffer[f->index];
        f->buffer[f->index] = value;
        f->sum += value;
    }

    // Cập nhật vị trí (vòng tròn)
    f->index = (f->index + 1) % PULSE_BUFFER_SIZE;

    // Trả về giá trị trung bình
    return f->sum / f->count;
}

/**
 * @brief Khởi tạo bộ phát hiện nhịp tim
 */
static void detector_init(pulse_detector_t *d)
{
    d->signal = 0;
    d->prev_signal = 0;
    d->threshold = PULSE_THRESHOLD;
    d->beat_detected = false;
    d->last_beat_time = 0;
    d->beat_interval = 0;
    d->bpm = 0;
    d->samples_since_last_beat = 0;
    d->rising = false;

    // Khởi tạo buffer BPM
    d->bpm_index = 0;
    d->bpm_sum = 0;
    memset(d->bpm_buffer, 0, sizeof(d->bpm_buffer));
}

/**
 * @brief Cập nhật ngưỡng tự động dựa trên tín hiệu
 *
 * Ngưỡng được tính = (giá trị đỉnh trung bình + giá trị đáy trung bình) / 2
 * Nhưng phiên bản đơn giản hơn: ngưỡng = giá trị trung bình * 1.1
 */
static void update_threshold(pulse_detector_t *d, int raw_value)
{
    static int max_value = 0;
    static int min_value = 4095; // ADC 12-bit max

    // Cập nhật max/min
    if (raw_value > max_value)
    {
        max_value = raw_value;
    }
    if (raw_value < min_value)
    {
        min_value = raw_value;
    }

    // Sau mỗi 2 giây, cập nhật lại ngưỡng
    static uint32_t last_threshold_update = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (now - last_threshold_update > 2000)
    {
        // Ngưỡng = (max - min) * 0.6 + min
        // Tức là 60% từ min lên max
        d->threshold = min_value + (int)((max_value - min_value) * 0.6f);

        // Đảm bảo ngưỡng nằm trong khoảng hợp lý
        if (d->threshold < 300)
            d->threshold = 300;
        if (d->threshold > 700)
            d->threshold = 700;

        ESP_LOGD(TAG, "Auto threshold: %d (min=%d, max=%d)",
                 d->threshold, min_value, max_value);

        // Reset max/min cho chu kỳ tiếp theo
        max_value = 0;
        min_value = 4095;
        last_threshold_update = now;
    }
}

/**
 * @brief Tính BPM từ khoảng cách giữa các nhịp
 */
static int calculate_bpm(uint32_t interval_ms)
{
    if (interval_ms == 0)
        return 0;

    // BPM = 60000 / interval (ms)
    int bpm = 60000 / interval_ms;

    // Giới hạn BPM trong khoảng hợp lý (30-200)
    if (bpm < 30)
        return 30;
    if (bpm > 200)
        return 200;
    return bpm;
}

/**
 * @brief Cập nhật BPM trung bình
 */
static void update_average_bpm(pulse_detector_t *d, int new_bpm)
{
    // Thêm vào buffer vòng tròn
    d->bpm_sum -= d->bpm_buffer[d->bpm_index];
    d->bpm_buffer[d->bpm_index] = new_bpm;
    d->bpm_sum += new_bpm;

    d->bpm_index = (d->bpm_index + 1) % 10;

    // Tính BPM trung bình từ buffer
    if (d->bpm_buffer[9] != 0)
    { // Đã đủ 10 mẫu
        d->bpm = d->bpm_sum / 10;
    }
    else
    {
        // Chưa đủ mẫu, lấy trung bình của các mẫu có
        int count = 0;
        int sum = 0;
        for (int i = 0; i < 10; i++)
        {
            if (d->bpm_buffer[i] != 0)
            {
                sum += d->bpm_buffer[i];
                count++;
            }
        }
        if (count > 0)
        {
            d->bpm = sum / count;
        }
        else
        {
            d->bpm = new_bpm;
        }
    }
}

bool pulse_init(void)
{
    ESP_LOGI(TAG, "Initializing Pulse Sensor...");

    // 1. Cấu hình ADC
    adc1_config_width(ADC_WIDTH_BIT_12);                  // 12-bit resolution (0-4095)
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN); // GPIO34 = ADC1_CH6

    // 2. Hiệu chỉnh ADC
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH_BIT_12,
        1100, // Default Vref
        &adc_chars);

    // 3. Khởi tạo bộ lọc và detector
    filter_init(&filter);
    detector_init(&pulse_detector);

    pulse_initialized = true;
    ESP_LOGI(TAG, "Pulse Sensor initialized");

    return true;
}

void pulse_update(void)
{
    if (!pulse_initialized)
    {
        return;
    }

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Đảm bảo tần số lấy mẫu ổn định
    if (current_time - last_update_time < (1000 / PULSE_SAMPLE_RATE))
    {
        return;
    }
    last_update_time = current_time;

    // 1. Đọc giá trị raw từ ADC
    int raw_value = adc1_get_raw(ADC1_CHANNEL_6);

    // 2. Lọc nhiễu bằng trung bình động
    int filtered_value = filter_add(&filter, raw_value);

    // 3. Cập nhật detector
    pulse_detector_t *d = &pulse_detector;
    d->prev_signal = d->signal;
    d->signal = filtered_value;
    d->samples_since_last_beat++;

    // 4. Tự động hiệu chỉnh ngưỡng
    update_threshold(d, raw_value);

    // 5. Phát hiện nhịp tim dựa trên ngưỡng và đạo hàm
    // Nguyên lý: Phát hiện khi tín hiệu vượt ngưỡng và đang ở cạnh lên

    // Kiểm tra xem tín hiệu có đang tăng không
    if (d->signal > d->prev_signal)
    {
        d->rising = true;
    }
    else
    {
        d->rising = false;
    }

    // Phát hiện nhịp: tín hiệu vượt ngưỡng VÀ đang ở cạnh xuống (sau đỉnh)
    // Đơn giản hơn: phát hiện khi tín hiệu vượt ngưỡng và trước đó dưới ngưỡng
    if (d->signal > d->threshold && d->prev_signal <= d->threshold)
    {
        // Đã phát hiện nhịp
        d->beat_detected = true;

        // Tính khoảng cách từ nhịp trước
        if (d->last_beat_time > 0)
        {
            d->beat_interval = current_time - d->last_beat_time;

            // Chỉ tính BPM nếu interval hợp lý
            if (d->beat_interval >= PULSE_MIN_INTERVAL &&
                d->beat_interval <= PULSE_MAX_INTERVAL)
            {

                int new_bpm = calculate_bpm(d->beat_interval);
                update_average_bpm(d, new_bpm);
            }
        }

        d->last_beat_time = current_time;
        d->samples_since_last_beat = 0;

        ESP_LOGD(TAG, "Beat detected! BPM: %d", d->bpm);
    }
    else
    {
        d->beat_detected = false;
    }

    // 6. Cập nhật dữ liệu hiện tại
    current_pulse.raw_value = raw_value;
    current_pulse.heart_rate = d->bpm;
    current_pulse.is_beat = d->beat_detected;
    current_pulse.last_beat_time = d->last_beat_time;

    // Tính độ tin cậy dựa trên số nhịp đã detect và độ ổn định
    if (d->bpm > 0)
    {
        if (d->bpm_buffer[9] != 0)
        { // Đã có 10 mẫu
            current_pulse.confidence = 0.9f;
        }
        else
        {
            current_pulse.confidence = 0.5f;
        }
    }
    else
    {
        current_pulse.confidence = 0.0f;
    }
}

bool pulse_read(pulse_data_t *data)
{
    if (!pulse_initialized || data == NULL)
    {
        return false;
    }

    // Copy dữ liệu hiện tại
    memcpy(data, &current_pulse, sizeof(pulse_data_t));

    return true;
}

int pulse_get_heart_rate(void)
{
    return pulse_detector.bpm;
}

bool pulse_is_beat(void)
{
    return pulse_detector.beat_detected;
}

void pulse_auto_threshold(int raw_value)
{
    // Có thể gọi hàm này để cập nhật ngưỡng nhanh hơn
    pulse_detector_t *d = &pulse_detector;

    static int max_val = 0;
    static int min_val = 4095;

    if (raw_value > max_val)
        max_val = raw_value;
    if (raw_value < min_val)
        min_val = raw_value;

    // Cập nhật mỗi 100 mẫu
    static int sample_count = 0;
    sample_count++;

    if (sample_count >= 100)
    {
        d->threshold = min_val + (int)((max_val - min_val) * 0.6f);
        sample_count = 0;
        max_val = 0;
        min_val = 4095;
    }
}

void pulse_reset(void)
{
    detector_init(&pulse_detector);
    filter_init(&filter);
    memset(&current_pulse, 0, sizeof(pulse_data_t));
    ESP_LOGI(TAG, "Pulse detector reset");
}