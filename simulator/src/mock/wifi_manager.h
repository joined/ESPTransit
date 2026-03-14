#pragma once

#include "esp_err.h"
#include <cstdint>

// Mock WiFi manager interface for simulator

// Initialize WiFi subsystem (call once at boot).
// If auto_connect is true, attempts to connect using stored credentials.
esp_err_t wifi_manager_init(bool auto_connect);

// Block until WiFi is connected or timeout. Use after init(true).
esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms = 15000);

// Connect to a network (blocking, with retries) - sets credentials and waits
esp_err_t wifi_manager_connect(const char *ssid, const char *password);
