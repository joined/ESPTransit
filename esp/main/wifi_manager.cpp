#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <cstring>

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRIES 3

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;
// TODO Refactor to state machine or enum, review flow
static bool s_connected = false;
static bool s_connecting = false;
static bool s_initialized = false;
static esp_netif_t *s_sta_netif = nullptr;
static bool s_needs_reset = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_connecting) {
            ESP_LOGI(TAG, "WiFi STA started, auto-connecting");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "WiFi STA started");
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_connecting && s_retry_count < WIFI_MAX_RETRIES) {
            s_retry_count++;
            ESP_LOGI(TAG, "Retry %d/%d connecting to WiFi...", s_retry_count, WIFI_MAX_RETRIES);
            esp_wifi_connect();
        } else if (s_connecting) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(bool auto_connect)
{
    if (s_initialized)
        return ESP_OK;

    s_wifi_event_group = xEventGroupCreate();

    // Set connecting flag before esp_wifi_start() so the STA_START handler
    // knows whether to auto-connect using the companion chip's stored credentials.
    s_connecting = auto_connect;
    s_retry_count = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");

    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr), TAG,
        "wifi event handler failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, nullptr), TAG,
        "ip event handler failed");

    // Check if we need to clear WiFi credentials (storage version reset)
    // This must be called BEFORE esp_wifi_set_mode() and esp_wifi_start()
    if (s_needs_reset) {
        ESP_LOGI(TAG, "Clearing WiFi credentials on C6 (storage version reset)");
        esp_err_t ret = esp_wifi_restore();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to restore WiFi: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "C6 WiFi credentials cleared");
        }
        s_needs_reset = false;
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized (auto_connect=%d)", auto_connect);
    return ESP_OK;
}

esp_err_t wifi_manager_wait_connected(uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }

    s_connecting = false;
    ESP_LOGE(TAG, "Timed out waiting for WiFi connection");
    return ESP_FAIL;
}

esp_err_t wifi_manager_scan(WiFiScanResult &result)
{
    memset(&result, 0, sizeof(result));

    // Ensure we're not in a connecting state
    s_connecting = false;
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));

    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = false;

    ESP_LOGI(TAG, "Starting WiFi scan...");
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));
        return err;
    }

    result.count = WIFI_MAX_APS;
    ESP_RETURN_ON_ERROR(esp_wifi_scan_get_ap_records(&result.count, result.aps), TAG, "get scan results failed");

    ESP_LOGI(TAG, "Scan found %d networks", result.count);
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    s_retry_count = 0;
    s_connecting = true;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    esp_wifi_disconnect();

    wifi_config_t wifi_config = {};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", password);
    wifi_config.sta.threshold.authmode = strlen(password) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "set config failed");

    esp_wifi_connect();

    ESP_LOGI(TAG, "Connecting to %s...", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to %s", ssid);
        return ESP_OK;
    }

    s_connecting = false;
    ESP_LOGE(TAG, "Failed to connect to %s", ssid);
    return ESP_FAIL;
}

esp_err_t wifi_manager_disconnect()
{
    s_connected = false;
    s_connecting = false;
    return esp_wifi_disconnect();
}

bool wifi_manager_is_connected()
{
    return s_connected;
}

void wifi_manager_request_reset()
{
    s_needs_reset = true;
    ESP_LOGI(TAG, "WiFi reset requested (will clear C6 credentials on next init)");
}
