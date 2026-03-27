#pragma once
#include <stdbool.h>

void wifi_manager_init(void);
void wifi_manager_connect(const char *ssid, const char *pass);
void wifi_manager_disconnect(void);

bool wifi_manager_is_connected(void);
const char* wifi_manager_get_ssid(void);