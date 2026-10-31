#pragma once

#include <stdbool.h>
#include "esp_err.h"

void bt_spp_init(void);
bool bt_spp_is_connected(void);
esp_err_t bt_spp_send(const uint8_t *data, size_t len);