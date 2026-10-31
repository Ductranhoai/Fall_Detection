#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t bt_manager_init(void);
bool bt_manager_is_connected(void);
esp_err_t bt_manager_notify_text(const char *text);
