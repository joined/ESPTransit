#pragma once

#include "app_constants.h"
#include "esp_err.h"
#include "http_client.h"
#include <vector>

enum class AppState {
    BOOT,
    WIFI_SETUP,
    WIFI_CONNECTING,
    STATION_SEARCH,
    DEPARTURES,
    SETTINGS,
    DEBUG,
};

struct AppConfig {
    bool configured;
    uint8_t rotation;   // 0=0°, 1=90°, 2=180°, 3=270°
    uint8_t brightness; // 10-100%
    bool split_mode;    // Show departures for multiple stations in split layout
    uint8_t font_size;  // 0=Small, 1=Medium, 2=Large, 255=board default
    std::vector<StationResult> stations;
};

// NVS namespace used for all config keys
static constexpr const char *NVS_NAMESPACE = "esptransit";

// Storage version - increment this to force a full reset of all stored data
// (both P4 app config and C6 WiFi credentials) on next boot.
// Useful for testing the WiFi setup flow.
static constexpr uint32_t STORAGE_VERSION = 0;

esp_err_t config_load(AppConfig &cfg);
esp_err_t config_save(const AppConfig &cfg);
esp_err_t config_clear();
bool storage_check_version();
esp_err_t storage_update_version();
