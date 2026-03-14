#pragma once

#include <cstddef>
#include <cstdint>

// App config defaults.
static constexpr bool APP_CONFIG_DEFAULT_CONFIGURED = false;
static constexpr uint8_t APP_CONFIG_DEFAULT_ROTATION = 0;
static constexpr uint8_t APP_CONFIG_DEFAULT_BRIGHTNESS = 80;
static constexpr bool APP_CONFIG_DEFAULT_SPLIT_MODE = false;
static constexpr uint8_t APP_CONFIG_DEFAULT_FONT_SIZE = 255; // 255 = use board default
static constexpr int APP_CONFIG_JSON_VERSION = 3;

// Station configuration constraints.
static constexpr size_t MAX_CONFIGURED_STATIONS = 4;
static constexpr size_t MAX_STATION_ID_LENGTH = 15;
static constexpr size_t MAX_STATION_NAME_LENGTH = 45;

// Shared request/config buffer capacities.
static constexpr size_t STATION_IDS_BUFFER_SIZE = 256;
static constexpr size_t STATION_SEARCH_QUERY_BUFFER_SIZE = 128;
static constexpr size_t HTTP_RESPONSE_BUFFER_SIZE = 256 * 1024; // 256KB

// Station search tuning.
static constexpr size_t STATION_SEARCH_MIN_QUERY_CHARS = 3;
static constexpr uint32_t STATION_SEARCH_DEBOUNCE_MS = 500;
static constexpr int STATION_SEARCH_API_RESULTS = 20;

// Departures query tuning.
static constexpr int DEPARTURES_API_DURATION_MINUTES = 60;
static constexpr int DEPARTURES_API_RESULTS = 30;

// Split mode constraints.
static constexpr size_t SPLIT_MODE_MIN_STATIONS = 2;
static constexpr size_t SPLIT_MODE_MAX_STATIONS = 4;

// Queue/task sizing.
static constexpr size_t APP_COMMAND_QUEUE_LENGTH = 16;
static constexpr size_t HTTP_REQUEST_QUEUE_LENGTH = 8;

// Boot/WiFi flow timings.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static constexpr uint32_t BOOT_CONNECT_FAILED_DELAY_MS = 2000;
static constexpr uint32_t BOOT_NO_CONFIG_DELAY_MS = 1500;
static constexpr int WIFI_SCAN_MAX_RESULTS = 20;

// API endpoint.
static constexpr const char *TRANSIT_API_BASE_URL = "https://api.suntrans.it";

// Screen refresh timers.
static constexpr uint32_t DEPARTURES_REFRESH_INTERVAL_MS = 30000;
static constexpr uint32_t SCREEN_CLOCK_UPDATE_INTERVAL_MS = 1000;
