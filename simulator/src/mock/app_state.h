#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "http_client.h"
#include <vector>

#define STORAGE_VERSION 0

#ifndef SIM_CONFIG_FILE_PATH
#define SIM_CONFIG_FILE_PATH "simulator/.simulator-config.json"
#endif

inline constexpr const char *kDefaultSimConfigFilePath = SIM_CONFIG_FILE_PATH;

enum class AppState { BOOT, WIFI_SETUP, WIFI_CONNECTING, STATION_SEARCH, DEPARTURES, SETTINGS, DEBUG };

struct AppConfig {
    bool configured;
    uint8_t rotation;
    uint8_t brightness;
    bool split_mode;
    uint8_t font_size; // 0=Small, 1=Medium, 2=Large, 255=board default
    std::vector<StationResult> stations;
};

bool config_load(AppConfig *config);
bool config_save(const AppConfig &config);
void config_clear();
void config_set_file_path(const char *path);
const char *config_get_file_path();
