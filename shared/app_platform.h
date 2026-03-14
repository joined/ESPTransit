#pragma once

#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ctime>
#include <string>

// Platform-agnostic WiFi network info for UI
struct UiWifiNetwork {
    std::string ssid;
    int8_t rssi;
    bool is_open;
};

// Platform-specific boot initialization result
struct PlatformBootResult {
    bool success;
    bool should_show_wifi_setup;
};

// Platform abstraction interface
// Different implementations for ESP32 hardware vs simulator
class AppPlatform {
  public:
    static void boot_init_early(AppConfig &config);

    // Low-level WiFi operations (platform-specific implementation)
    // Returns number of networks found, or -1 on error
    static int wifi_do_scan(UiWifiNetwork *results, int max_count, const char **current_ssid);

    // Reconnect to previous WiFi network (if any). Returns true on success.
    static bool wifi_do_reconnect();

    // Sync time via SNTP (no-op on simulator)
    static void sntp_sync();

    // Settings operations (platform-specific)
    // old_rotation is needed for ESP to determine if reboot is required
    static void handle_rotation_change(AppConfig &config, uint8_t old_rotation);
    static void handle_brightness_change(AppConfig &config);
    static const char *get_current_ssid();
    static bool is_wifi_connected();

    // True when simulator UI should render deterministic labels for screenshot tests.
    static bool is_ui_test_mode();

    // Returns the app version string; in UI test mode returns a fixed label.
    static const char *get_version_string();

    // Returns the current wall-clock time; in UI test mode returns a fixed timestamp.
    static time_t get_wall_clock_time();
};
