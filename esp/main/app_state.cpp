#include "app_state.h"
#include "app_config_codec.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string>

static const char *TAG = "app_state";
static constexpr const char *CONFIG_JSON_KEY = "cfg_json";

esp_err_t config_load(AppConfig &cfg)
{
    app_config_codec::set_defaults(cfg);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No config found in NVS");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t json_len = 0;
    err = nvs_get_str(handle, CONFIG_JSON_KEY, nullptr, &json_len);
    if (err == ESP_ERR_NVS_NOT_FOUND || json_len == 0) {
        nvs_close(handle);
        ESP_LOGI(TAG, "No config JSON found in NVS");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Failed to read config JSON length: %s", esp_err_to_name(err));
        return err;
    }

    std::string json(json_len > 0 ? json_len - 1 : 0, '\0');
    err = nvs_get_str(handle, CONFIG_JSON_KEY, json.data(), &json_len);
    if (err != ESP_OK) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Failed to read config JSON: %s", esp_err_to_name(err));
        return err;
    }

    DeserializationError parse_err = app_config_codec::deserialize(json, cfg);
    if (parse_err) {
        nvs_close(handle);
        ESP_LOGW(TAG, "Failed to parse config JSON, using defaults");
        return ESP_OK;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Config loaded: configured=%d, split_mode=%d, stations=%d", cfg.configured, cfg.split_mode,
             (int)cfg.stations.size());
    return ESP_OK;
}

esp_err_t config_save(const AppConfig &cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    std::string json;
    app_config_codec::serialize(cfg, json);

    err = nvs_set_str(handle, CONFIG_JSON_KEY, json.c_str());
    if (err != ESP_OK) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Failed to write config JSON: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Config saved: split_mode=%d, stations=%d", cfg.split_mode, (int)cfg.stations.size());
    }
    return err;
}

esp_err_t config_clear()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;

    nvs_erase_all(handle);
    err = nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Config cleared");
    return err;
}

bool storage_check_version()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "First boot - storage needs initialization");
        return true; // Reset needed (first boot)
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for version check: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t stored_version = 0;
    err = nvs_get_u32(handle, "stor_ver", &stored_version);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND || stored_version != STORAGE_VERSION) {
        ESP_LOGI(TAG, "Storage version mismatch (stored=%lu, current=%lu) - triggering reset", stored_version,
                 STORAGE_VERSION);
        return true; // Reset needed
    }

    ESP_LOGI(TAG, "Storage version OK (v%lu)", STORAGE_VERSION);
    return false; // No reset needed
}

esp_err_t storage_update_version()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS to update version: %s", esp_err_to_name(err));
        return err;
    }

    nvs_set_u32(handle, "stor_ver", STORAGE_VERSION);
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Storage version updated to v%lu", STORAGE_VERSION);
    }
    return err;
}
