#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"

#define WIFI_MAX_APS 20

struct WiFiScanResult {
    wifi_ap_record_t aps[WIFI_MAX_APS];
    uint16_t count;
};

// Initialize WiFi subsystem (call once at boot).
// If auto_connect is true, STA_START will trigger esp_wifi_connect()
// using whatever credentials the companion chip already has stored.
esp_err_t wifi_manager_init(bool auto_connect);

// Block until WiFi is connected or timeout. Use after init(true).
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms = 15000);

// Scan for available networks (blocking)
esp_err_t wifi_manager_scan(WiFiScanResult &result);

// Connect to a network (blocking, with retries)
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

// Disconnect from current network
esp_err_t wifi_manager_disconnect();

// Check if currently connected
bool wifi_manager_is_connected();

// Request WiFi credentials to be cleared on next init (used for storage version resets)
void wifi_manager_request_reset();
