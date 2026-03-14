#include "app_state.h"
#include "app_config_codec.h"
#include "sim_log.h"
#include <cstdio>
#include <fstream>
#include <iterator>

static const char *TAG = "config";
static std::string s_config_file_path = kDefaultSimConfigFilePath;

void config_set_file_path(const char *path)
{
    if (path && path[0] != '\0') {
        s_config_file_path = path;
        sim_log_print(SIM_LOG_INFO, TAG, "Using config file %s", s_config_file_path.c_str());
    } else {
        s_config_file_path = kDefaultSimConfigFilePath;
        sim_log_print(SIM_LOG_INFO, TAG, "Using default config file %s", s_config_file_path.c_str());
    }
}

const char *config_get_file_path()
{
    return s_config_file_path.c_str();
}

bool config_load(AppConfig *config)
{
    if (!config) {
        return false;
    }

    app_config_codec::set_defaults(*config);

    const char *config_file_path = config_get_file_path();
    std::ifstream in(config_file_path);
    if (!in.is_open()) {
        sim_log_print(SIM_LOG_INFO, TAG, "No saved config at %s, using defaults", config_file_path);
        return true;
    }

    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (json.empty()) {
        sim_log_print(SIM_LOG_WARN, TAG, "Config file %s is empty, using defaults", config_file_path);
        return true;
    }

    const DeserializationError parse_err = app_config_codec::deserialize(json, *config);
    if (parse_err) {
        sim_log_print(SIM_LOG_WARN, TAG, "Failed to parse config file %s (%s), using defaults", config_file_path,
                      parse_err.c_str());
        return true;
    }

    sim_log_print(SIM_LOG_INFO, TAG, "Loaded from %s: stations=%d, rotation=%d°, brightness=%d%%, split_mode=%d",
                  config_file_path, (int)config->stations.size(), config->rotation * 90, config->brightness,
                  config->split_mode ? 1 : 0);
    return true;
}

bool config_save(const AppConfig &config)
{
    const char *config_file_path = config_get_file_path();
    std::string json;
    app_config_codec::serialize(config, json);

    std::ofstream out(config_file_path, std::ios::trunc);
    if (!out.is_open()) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Failed to open %s for writing", config_file_path);
        return false;
    }

    out << json;
    if (!out.good()) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Failed writing config to %s", config_file_path);
        return false;
    }

    sim_log_print(SIM_LOG_INFO, TAG, "Saved to %s: stations=%d, rotation=%d°, brightness=%d%%, split_mode=%d",
                  config_file_path, (int)config.stations.size(), config.rotation * 90, config.brightness,
                  config.split_mode ? 1 : 0);
    return true;
}

void config_clear()
{
    const char *config_file_path = config_get_file_path();
    if (std::remove(config_file_path) == 0) {
        sim_log_print(SIM_LOG_INFO, TAG, "Cleared config file %s", config_file_path);
    } else {
        sim_log_print(SIM_LOG_INFO, TAG, "No config file to clear at %s", config_file_path);
    }
}
