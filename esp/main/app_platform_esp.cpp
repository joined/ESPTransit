#include "app_platform.h"
#include "app_manager.h"
#include "app_state.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "wifi_manager.h"
#include "ui/common.h"
#include <cstring>
#include <ctime>

static const char *TAG = "app_platform";

static void init_sntp()
{
    ESP_LOGI(TAG, "Initializing SNTP");

    // Stop SNTP if already running (e.g., when switching WiFi networks)
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already running, stopping first");
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    // Set timezone to Berlin
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    // Wait for time to be synchronized (max 10 seconds)
    ESP_LOGI(TAG, "Waiting for SNTP time sync...");
    time_t now = 0;
    struct tm timeinfo = {};
    int retry = 0;
    const int max_retry = 50; // 50 * 200ms = 10 seconds

    while (timeinfo.tm_year < (2020 - 1900) && retry < max_retry) {
        vTaskDelay(pdMS_TO_TICKS(200));
        time(&now);
        localtime_r(&now, &timeinfo);
        retry++;
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "SNTP sync timeout - time may be incorrect");
    } else {
        ESP_LOGI(TAG, "SNTP time synchronized");
    }
}

void AppPlatform::boot_init_early(AppConfig &config)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Check storage version and perform reset if needed
    if (storage_check_version()) {
        ESP_LOGI(TAG, "Storage version changed - performing full reset");
        config_clear();
        wifi_manager_request_reset();
        storage_update_version();
    }

    // Load config
    config_load(config);

    // Start display (rotation mode determines HW vs SW rotation)
    bool needs_sw_rotation = (config.rotation == 1 || config.rotation == 3);
    ESP_LOGI(TAG, "Boot: rotation=%d, sw_rotate=%s", config.rotation, needs_sw_rotation ? "true" : "false");
    bsp_display_start(needs_sw_rotation);
}

int AppPlatform::wifi_do_scan(UiWifiNetwork *results, int max_count, const char **current_ssid)
{
    WiFiScanResult scan_result;
    esp_err_t err = wifi_manager_scan(scan_result);
    if (err != ESP_OK) {
        return -1;
    }

    // Get current WiFi SSID for highlighting
    static wifi_config_t wifi_cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
        *current_ssid = (const char *)wifi_cfg.sta.ssid;
    }

    // Convert to platform-agnostic UI type
    int count = scan_result.count < max_count ? scan_result.count : max_count;
    for (int i = 0; i < count; i++) {
        results[i].ssid = (const char *)scan_result.aps[i].ssid;
        results[i].rssi = scan_result.aps[i].rssi;
        results[i].is_open = (scan_result.aps[i].authmode == WIFI_AUTH_OPEN);
    }

    return count;
}

bool AppPlatform::wifi_do_reconnect()
{
    // Check if we have stored credentials and aren't already connected
    wifi_config_t wifi_cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK || strlen((const char *)wifi_cfg.sta.ssid) == 0 ||
        wifi_manager_is_connected()) {
        return false;
    }

    ESP_LOGI(TAG, "Reconnecting to previous WiFi network");
    const char *ssid = (const char *)wifi_cfg.sta.ssid;
    const char *password = (const char *)wifi_cfg.sta.password;
    esp_err_t err = wifi_manager_connect(ssid, password);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to reconnect to WiFi");
        return false;
    }

    return true;
}

void AppPlatform::sntp_sync()
{
    init_sntp();
}

void AppPlatform::handle_rotation_change(AppConfig &config, uint8_t old_rotation)
{
    uint8_t new_rotation = config.rotation; // Already updated by caller
    ESP_LOGI(TAG, "Rotation change requested: %d (old: %d)", new_rotation, old_rotation);

    // Check if rotation mode changed (HW vs SW rotation)
    bool old_needs_sw = (old_rotation == 1 || old_rotation == 3); // 90° or 270°
    bool new_needs_sw = (new_rotation == 1 || new_rotation == 3);

    if (old_needs_sw != new_needs_sw) {
        // Rotation mode changed - need to reboot to reinitialize display
        ESP_LOGI(TAG, "Rotation mode changed - rebooting...");

        // Turn off backlight to avoid flash during reboot
        bsp_display_backlight_off();
        vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay for backlight to fade

        esp_restart();
    } else {
        // Same rotation mode - can apply immediately
        bsp_display_lock(0);
        lv_display_t *disp = lv_display_get_default();
        bsp_display_rotate(disp, static_cast<lv_disp_rotation_t>(new_rotation));

        int32_t h_res = lv_display_get_horizontal_resolution(disp);
        int32_t v_res = lv_display_get_vertical_resolution(disp);
        ESP_LOGI(TAG, "Display after rotation change: %ldx%ld", h_res, v_res);

        bsp_display_unlock();
    }
}

void AppPlatform::handle_brightness_change(AppConfig &config)
{
    // Config already updated by caller
    ESP_LOGI(TAG, "Brightness change: %d%%", config.brightness);

    // Apply brightness immediately (config save is debounced by caller)
    bsp_display_brightness_set(config.brightness);
}

const char *AppPlatform::get_current_ssid()
{
    static wifi_config_t wifi_cfg = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
        return (const char *)wifi_cfg.sta.ssid;
    }
    return "";
}

bool AppPlatform::is_wifi_connected()
{
    return wifi_manager_is_connected();
}

bool AppPlatform::is_ui_test_mode()
{
    return false;
}

const char *AppPlatform::get_version_string()
{
    return esp_app_get_description()->version;
}

time_t AppPlatform::get_wall_clock_time()
{
    return time(nullptr);
}
