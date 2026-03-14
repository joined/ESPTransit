#include "wifi_manager.h"
#include "app_state.h"
#include "esp_log.h"
#include "sim_timing.h"
#include <cstring>

static const char *TAG = "wifi_manager";

// Mock WiFi state (shared with app_platform_sim.cpp)
extern bool s_wifi_connected;
extern char s_wifi_ssid[33];

esp_err_t wifi_manager_init(bool auto_connect)
{
    ESP_LOGI(TAG, "WiFi manager init (auto_connect=%d)", auto_connect);

    // If auto-connect, simulate connecting to saved network
    if (auto_connect) {
        // Check if we have saved config with WiFi credentials
        AppConfig config;
        if (config_load(&config) && config.configured) {
            s_wifi_connected = true;
            strncpy(s_wifi_ssid, "HomeWiFi", sizeof(s_wifi_ssid) - 1);
            s_wifi_ssid[sizeof(s_wifi_ssid) - 1] = '\0';
            ESP_LOGI(TAG, "Auto-connect enabled, will connect to saved network");
        }
    }

    return ESP_OK;
}

esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms)
{
    ESP_LOGI(TAG, "Waiting for WiFi connection (timeout=%lu ms)...", timeout_ms);

    // Simulate connection delay
    sim_delay_ms(1500);

    // Check if we're connected
    if (s_wifi_connected) {
        ESP_LOGI(TAG, "WiFi connected to '%s'", s_wifi_ssid);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "WiFi connection failed (no stored credentials)");
    return ESP_FAIL;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Connecting to '%s'...", ssid);
    sim_delay_ms(2000); // Simulate connection delay

    // Simulate connection failure for "ErrorNetwork"
    if (strcmp(ssid, "ErrorNetwork") == 0) {
        sim_delay_ms(1000); // Additional delay before failure
        ESP_LOGE(TAG, "Connection failed: authentication timeout (simulated failure)");
        return ESP_FAIL;
    }

    // Validate password for secured networks
    bool is_open = (strcmp(ssid, "PublicWiFi") == 0 || strcmp(ssid, "CafeGuest") == 0);
    if (!is_open && strlen(password) < 8) {
        ESP_LOGE(TAG, "Connection failed: password too short");
        return ESP_FAIL;
    }

    // Save credentials and mark as connected
    s_wifi_connected = true;
    strncpy(s_wifi_ssid, ssid, sizeof(s_wifi_ssid) - 1);
    s_wifi_ssid[sizeof(s_wifi_ssid) - 1] = '\0';
    ESP_LOGI(TAG, "Connected to '%s'", ssid);
    return ESP_OK;
}
