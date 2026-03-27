#include "../components\esp_console\esp_console.h"
#include "wifi_manager.h"
#include "esp_wifi.h"

static int cmd_wifi_scan(int argc, char **argv)
{
    wifi_scan_config_t scan_config = {0};
    esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_num = 20;
    wifi_ap_record_t ap_records[20];

    esp_wifi_scan_get_ap_records(&ap_num, ap_records);

    for (int i = 0; i < ap_num; i++) {
        printf("%s (%d)\n", ap_records[i].ssid, ap_records[i].rssi);
    }
    return 0;
}

static int cmd_wifi_connect(int argc, char **argv)
{
    if (argc < 3) {
        printf("wifi_connect <ssid> <pass>\n");
        return 1;
    }

    wifi_manager_connect(argv[1], argv[2]);
    return 0;
}

static int cmd_wifi_status(int argc, char **argv)
{
    if (wifi_manager_is_connected()) {
        printf("Connected to %s\n", wifi_manager_get_ssid());
    } else {
        printf("Not connected\n");
    }
    return 0;
}

static int cmd_wifi_disconnect(int argc, char **argv)
{
    wifi_manager_disconnect();
    printf("Disconnected\n");
    return 0;
}

void wifi_cli_register(void)
{
    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "wifi_scan",
        .help = "Scan WiFi",
        .func = cmd_wifi_scan,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "wifi_connect",
        .help = "wifi_connect <ssid> <pass>",
        .func = cmd_wifi_connect,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "wifi_status",
        .help = "WiFi status",
        .func = cmd_wifi_status,
    });

    esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "wifi_disconnect",
        .help = "Disconnect WiFi",
        .func = cmd_wifi_disconnect,
    });
}