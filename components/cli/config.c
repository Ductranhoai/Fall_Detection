#include "config.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static const char *TAG = "CONFIG";
static system_config_t system_config;
static const char *CONFIG_FILE = "/fatfs/system_config.bin";

// Default configuration
static void set_default_config(void)
{
    // Fall detection defaults
    system_config.fall.impact_threshold = 2.5f;
    system_config.fall.orientation_threshold = 45.0f;
    system_config.fall.inactivity_threshold = 2.0f;
    system_config.fall.fall_timeout = 10.0f;
    system_config.fall.sample_rate_hz = 50;
    system_config.fall.auto_start = true;

    // I2C defaults
    system_config.i2c.mode = I2C_MODE_MASTER;
    system_config.i2c.sda_io_num = 21;
    system_config.i2c.scl_io_num = 22;
    system_config.i2c.sda_pullup_en = GPIO_PULLUP_ENABLE;
    system_config.i2c.scl_pullup_en = GPIO_PULLUP_ENABLE;
    system_config.i2c.master.clk_speed = 400000;
    system_config.i2c_port = I2C_NUM_0;

    // System defaults
    system_config.enable_log_to_file = true;
    strcpy(system_config.log_file_path, "/fatfs/fall_log.txt");
}

esp_err_t config_init(void)
{
    // Set default config first
    set_default_config();

    // Try to load existing config
    FILE *f = fopen(CONFIG_FILE, "rb");
    if (f)
    {
        size_t bytes_read = fread(&system_config, sizeof(system_config_t), 1, f);
        fclose(f);
        if (bytes_read == 1)
        {
            ESP_LOGI(TAG, "Configuration loaded from %s", CONFIG_FILE);
        }
        else
        {
            ESP_LOGW(TAG, "Failed to read config file, using defaults");
            config_save();
        }
    }
    else
    {
        ESP_LOGW(TAG, "No config file found, using defaults and saving");
        config_save();
    }

    ESP_LOGI(TAG, "Configuration initialized");
    config_print();

    return ESP_OK;
}

esp_err_t config_save(void)
{
    FILE *f = fopen(CONFIG_FILE, "wb");
    if (!f)
    {
        ESP_LOGE(TAG, "Failed to open config file for writing");
        return ESP_ERR_NOT_FOUND;
    }

    size_t bytes_written = fwrite(&system_config, sizeof(system_config_t), 1, f);
    fclose(f);

    if (bytes_written != 1)
    {
        ESP_LOGE(TAG, "Failed to write config file");
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Configuration saved to %s", CONFIG_FILE);
    return ESP_OK;
}

esp_err_t config_load(void)
{
    FILE *f = fopen(CONFIG_FILE, "rb");
    if (!f)
    {
        ESP_LOGW(TAG, "Config file not found");
        return ESP_ERR_NOT_FOUND;
    }

    size_t bytes_read = fread(&system_config, sizeof(system_config_t), 1, f);
    fclose(f);

    if (bytes_read != 1)
    {
        ESP_LOGE(TAG, "Failed to read config file");
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t config_reset_to_default(void)
{
    set_default_config();
    esp_err_t ret = config_save();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Configuration reset to defaults");
        config_print();
    }
    return ret;
}

system_config_t *config_get(void)
{
    return &system_config;
}

esp_err_t config_set_fall_param(const char *param, float value)
{
    if (strcmp(param, "impact") == 0)
    {
        system_config.fall.impact_threshold = value;
        // Validate
        if (system_config.fall.impact_threshold < 0.5)
            system_config.fall.impact_threshold = 0.5;
        if (system_config.fall.impact_threshold > 8.0)
            system_config.fall.impact_threshold = 8.0;
    }
    else if (strcmp(param, "orientation") == 0)
    {
        system_config.fall.orientation_threshold = value;
        if (system_config.fall.orientation_threshold < 10)
            system_config.fall.orientation_threshold = 10;
        if (system_config.fall.orientation_threshold > 90)
            system_config.fall.orientation_threshold = 90;
    }
    else if (strcmp(param, "inactivity") == 0)
    {
        system_config.fall.inactivity_threshold = value;
        if (system_config.fall.inactivity_threshold < 0.5)
            system_config.fall.inactivity_threshold = 0.5;
        if (system_config.fall.inactivity_threshold > 10)
            system_config.fall.inactivity_threshold = 10;
    }
    else if (strcmp(param, "timeout") == 0)
    {
        system_config.fall.fall_timeout = value;
        if (system_config.fall.fall_timeout < 5)
            system_config.fall.fall_timeout = 5;
        if (system_config.fall.fall_timeout > 30)
            system_config.fall.fall_timeout = 30;
    }
    else if (strcmp(param, "sample_rate") == 0)
    {
        system_config.fall.sample_rate_hz = (int)value;
        if (system_config.fall.sample_rate_hz < 10)
            system_config.fall.sample_rate_hz = 10;
        if (system_config.fall.sample_rate_hz > 200)
            system_config.fall.sample_rate_hz = 200;
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    return config_save();
}

esp_err_t config_set_i2c_param(const char *param, int value)
{
    if (strcmp(param, "sda") == 0)
    {
        system_config.i2c.sda_io_num = value;
    }
    else if (strcmp(param, "scl") == 0)
    {
        system_config.i2c.scl_io_num = value;
    }
    else if (strcmp(param, "speed") == 0)
    {
        system_config.i2c.master.clk_speed = value;
        if (system_config.i2c.master.clk_speed < 100000)
            system_config.i2c.master.clk_speed = 100000;
        if (system_config.i2c.master.clk_speed > 400000)
            system_config.i2c.master.clk_speed = 400000;
    }
    else
    {
        return ESP_ERR_INVALID_ARG;
    }

    return config_save();
}

void config_print(void)
{
    printf("\n========== System Configuration ==========\n");
    printf("\n--- Fall Detection ---\n");
    printf("  Impact threshold:      %.2f g\n", system_config.fall.impact_threshold);
    printf("  Orientation threshold: %.2f deg\n", system_config.fall.orientation_threshold);
    printf("  Inactivity threshold:  %.2f sec\n", system_config.fall.inactivity_threshold);
    printf("  Fall timeout:          %.2f sec\n", system_config.fall.fall_timeout);
    printf("  Sample rate:           %d Hz\n", system_config.fall.sample_rate_hz);
    printf("  Auto start:            %s\n", system_config.fall.auto_start ? "Yes" : "No");

    printf("\n--- I2C Configuration ---\n");
    printf("  SDA pin:               %d\n", system_config.i2c.sda_io_num);
    printf("  SCL pin:               %d\n", system_config.i2c.scl_io_num);
    printf("  Clock speed:           %lu Hz\n", system_config.i2c.master.clk_speed);
    printf("  I2C port:              %d\n", system_config.i2c_port);

    printf("\n--- System ---\n");
    printf("  Log to file:           %s\n", system_config.enable_log_to_file ? "Yes" : "No");
    printf("  Log file path:         %s\n", system_config.log_file_path);
    printf("==========================================\n");
}