#pragma once
#include <stdbool.h>

bool wifi_storage_save(const char *ssid, const char *pass);
bool wifi_storage_load(char *ssid, char *pass);