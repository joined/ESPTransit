#include "app_platform.h"
#include "esp_log.h"
#include "app_state.h"
#include "bsp/esp-bsp.h"
#include "sim_timing.h"
#include "sim_env_utils.h"
#include <cstdlib>
#include <cstring>

static const char *TAG = "app_platform";

// Mock WiFi state (shared with wifi_manager.cpp)
bool s_wifi_connected = false;
char s_wifi_ssid[33] = "";

// Mock WiFi networks for scanning
static const struct {
    const char *ssid;
    int8_t rssi;
    bool is_open;
} s_mock_networks[] = {{"HomeWiFi", -45, false},
                       {"ErrorNetwork", -52, false},
                       {"PublicWiFi", -68, true},
                       {"CafeGuest", -75, true},
                       {"Neighbor5G", -82, false}};

void AppPlatform::boot_init_early(AppConfig &config)
{
    config_load(&config);

    // Start display (creates SDL window and LVGL timer task)
    bsp_display_start();

    ESP_LOGI(TAG, "=== ESPTransit Simulator Starting ===");
}

int AppPlatform::wifi_do_scan(UiWifiNetwork *results, int max_count, const char **current_ssid)
{
    ESP_LOGI(TAG, "Platform scanning networks...");
    sim_delay_ms(1000); // Simulate scan delay

    // Return current SSID if connected
    *current_ssid = s_wifi_connected ? s_wifi_ssid : nullptr;

    // Return mock networks
    int count = sizeof(s_mock_networks) / sizeof(s_mock_networks[0]);
    if (count > max_count)
        count = max_count;

    for (int i = 0; i < count; i++) {
        results[i].ssid = s_mock_networks[i].ssid;
        results[i].rssi = s_mock_networks[i].rssi;
        results[i].is_open = s_mock_networks[i].is_open;
    }

    ESP_LOGI(TAG, "Found %d networks", count);
    return count;
}

bool AppPlatform::wifi_do_reconnect()
{
    // Check if we have a network to reconnect to
    if (strlen(s_wifi_ssid) == 0) {
        return false;
    }

    // If already connected, just verify and return (with delay for spinner visibility)
    if (s_wifi_connected) {
        ESP_LOGI(TAG, "Already connected to %s, verifying...", s_wifi_ssid);
        sim_delay_ms(1500); // Simulate verification delay
        return true;
    }

    // Not connected, attempt reconnection
    ESP_LOGI(TAG, "Reconnecting to previous network: %s", s_wifi_ssid);
    sim_delay_ms(1500); // Simulate reconnection delay
    s_wifi_connected = true;
    return true;
}

void AppPlatform::sntp_sync()
{
    // Simulate SNTP sync delay
    ESP_LOGI(TAG, "SNTP sync (simulated)");
    sim_delay_ms(1000);
}

void AppPlatform::handle_rotation_change(AppConfig &config, uint8_t old_rotation)
{
    (void)old_rotation; // Simulator doesn't need to check for mode changes
    // Config already updated by caller, just apply the hardware change
    bsp_display_lock(0);
    lv_display_t *disp = lv_display_get_default();
    bsp_display_rotate(disp, static_cast<lv_disp_rotation_t>(config.rotation));
    bsp_display_unlock();
}

void AppPlatform::handle_brightness_change(AppConfig &config)
{
    // Config already updated by caller, just apply the hardware change
    bsp_display_brightness_set(config.brightness);
}

const char *AppPlatform::get_current_ssid()
{
    return s_wifi_connected ? s_wifi_ssid : "";
}

bool AppPlatform::is_wifi_connected()
{
    return s_wifi_connected;
}

bool AppPlatform::is_ui_test_mode()
{
    return env_flag_enabled("UI_TEST");
}

const char *AppPlatform::get_version_string()
{
    if (env_flag_enabled("UI_TEST")) {
        return "ui-test";
    }
    return APP_VERSION;
}

time_t AppPlatform::get_wall_clock_time()
{
    return sim_get_reference_time();
}
