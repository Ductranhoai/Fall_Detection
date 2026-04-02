/**
 * Wifi : core wifi
 */
#include "wifi_manager.h"
#include "wifi_storage.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi_mgr";

static bool s_connected = false;
static bool s_connecting = false;
static char s_ssid[32] = {0};
static char s_ip[16] = {0};
static int s_reconnect_count = 0;
static bool s_auto_reconnect_enabled = true;

// Hàm chuyển đổi reason code thành string
static const char* wifi_disconnect_reason_str(uint8_t reason)
{
    switch(reason) {
        case WIFI_REASON_AUTH_EXPIRE: return "Authentication expired";
        case WIFI_REASON_AUTH_LEAVE: return "Authentication leave";
        case WIFI_REASON_ASSOC_EXPIRE: return "Association expired";
        case WIFI_REASON_ASSOC_TOOMANY: return "Too many associations";
        case WIFI_REASON_NOT_AUTHED: return "Not authenticated";
        case WIFI_REASON_NOT_ASSOCED: return "Not associated";
        case WIFI_REASON_ASSOC_LEAVE: return "Association leave";
        case WIFI_REASON_ASSOC_NOT_AUTHED: return "Association not authenticated";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "Power capability bad";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "Supported channel bad";
        case WIFI_REASON_IE_INVALID: return "Invalid IE";
        case WIFI_REASON_MIC_FAILURE: return "MIC failure";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4-way handshake timeout";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "Group key update timeout";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE in 4-way differs";
        case WIFI_REASON_GROUP_CIPHER_INVALID: return "Group cipher invalid";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "Pairwise cipher invalid";
        case WIFI_REASON_AKMP_INVALID: return "AKMP invalid";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "Unsupported RSN IE version";
        case WIFI_REASON_INVALID_RSN_IE_CAP: return "Invalid RSN IE cap";
        case WIFI_REASON_802_1X_AUTH_FAILED: return "802.1X authentication failed";
        case WIFI_REASON_CIPHER_SUITE_REJECTED: return "Cipher suite rejected";
        case WIFI_REASON_BEACON_TIMEOUT: return "Beacon timeout";
        case WIFI_REASON_NO_AP_FOUND: return "No AP found";
        case WIFI_REASON_AUTH_FAIL: return "Authentication failed - WRONG PASSWORD";
        case WIFI_REASON_ASSOC_FAIL: return "Association failed";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "Handshake timeout";
        default: return "Unknown reason";
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "📡 WiFi station started");
            s_connecting = false;
            
            // Auto connect khi WiFi started nếu có credentials
            if (s_auto_reconnect_enabled) {
                char ssid[32], pass[64];
                if (wifi_storage_load(ssid, pass) && strlen(ssid) > 0) {
                    ESP_LOGI(TAG, "Auto-connecting to saved network: %s", ssid);
                    wifi_manager_connect(ssid, pass);
                }
            }
        }
        else if (event_id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "🔗 Connected to AP");
            s_connecting = false;
            s_reconnect_count = 0;
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            s_connecting = false;
            wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*)event_data;
            
            ESP_LOGW(TAG, "========================================");
            ESP_LOGW(TAG, "❌ Disconnected from AP");
            ESP_LOGW(TAG, "   SSID: %s", disconnected->ssid);
            ESP_LOGW(TAG, "   Reason: %d - %s", 
                     disconnected->reason,
                     wifi_disconnect_reason_str(disconnected->reason));
            
            // Phân tích lý do
            if (disconnected->reason == WIFI_REASON_AUTH_FAIL) {
                ESP_LOGE(TAG, "   🔐 WRONG PASSWORD! Clearing saved credentials");
                // Xóa credentials sai
                wifi_storage_save("", "");
                s_auto_reconnect_enabled = false;
            } 
            else if (disconnected->reason == WIFI_REASON_NO_AP_FOUND) {
                ESP_LOGE(TAG, "   🔍 SSID NOT FOUND! Clearing saved credentials");
                wifi_storage_save("", "");
                s_auto_reconnect_enabled = false;
            }
            else if (disconnected->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                     disconnected->reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
                ESP_LOGE(TAG, "   ⏱️  HANDSHAKE TIMEOUT! Possible wrong password");
            }
            else if (disconnected->reason == WIFI_REASON_BEACON_TIMEOUT) {
                ESP_LOGE(TAG, "   📡 BEACON TIMEOUT! Signal too weak");
            }
            
            ESP_LOGW(TAG, "========================================");
            
            // Auto reconnect chỉ khi có credentials hợp lệ và chưa quá số lần
            if (s_auto_reconnect_enabled && s_reconnect_count < 3) {
                s_reconnect_count++;
                ESP_LOGI(TAG, "🔄 Auto-reconnecting attempt %d/3...", s_reconnect_count);
                esp_wifi_connect();
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        s_connecting = false;
        s_reconnect_count = 0;
        s_auto_reconnect_enabled = true;  // Đã kết nối thành công, bật auto reconnect
        
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "✅ WiFi Connected Successfully!");
        ESP_LOGI(TAG, "   SSID: %s", s_ssid);
        ESP_LOGI(TAG, "   IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "   Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
        ESP_LOGI(TAG, "========================================");
        
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_manager_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "✓ NVS initialized");
    
    // Initialize network
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    ESP_LOGI(TAG, "✓ Network interface initialized");
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "✓ WiFi driver initialized");
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // Set mode and start
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "✓ WiFi started in station mode");
    ESP_LOGI(TAG, "========================================");
    
    // Auto-connect will be triggered in WIFI_EVENT_STA_START
}

void wifi_manager_connect(const char *ssid, const char *pass)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "📡 Manual connect to: %s", ssid);
    ESP_LOGI(TAG, "   Password: %s", pass ? "********" : "No password");
    
    // Disconnect hiện tại
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Tạo config mới
    wifi_config_t cfg = {0};
    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);
    
    // Lưu credentials vào NVS
    wifi_storage_save(ssid, pass);
    
    // Set config
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return;
    }
    
    // Reset state
    s_connected = false;
    s_connecting = true;
    s_reconnect_count = 0;
    s_auto_reconnect_enabled = true;
    s_ip[0] = '\0';
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    
    // Connect
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect: %s", esp_err_to_name(ret));
        s_connecting = false;
        return;
    }
    
    ESP_LOGI(TAG, "Connection initiated...");
}

void wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "🔌 Manual disconnect WiFi");
    esp_wifi_disconnect();
    s_connected = false;
    s_connecting = false;
    s_ssid[0] = '\0';
    s_ip[0] = '\0';
    s_reconnect_count = 0;
    // Không xóa credentials, chỉ disconnect
}

void wifi_manager_clear_credentials(void)
{
    ESP_LOGI(TAG, "🗑️  Clearing saved WiFi credentials");
    wifi_storage_save("", "");
    esp_wifi_disconnect();
    s_connected = false;
    s_connecting = false;
    s_ssid[0] = '\0';
    s_ip[0] = '\0';
    s_reconnect_count = 0;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

bool wifi_manager_is_connecting(void)
{
    return s_connecting;
}

const char* wifi_manager_get_ssid(void)
{
    return s_ssid;
}

const char* wifi_manager_get_ip(void)
{
    return s_ip;
}