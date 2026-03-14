#pragma once

#include "app_constants.h"
#include "http_client.h"
#include <ArduinoJson.h>
#include <string>
#include <utility>

namespace app_config_codec {

template <typename TConfig> inline void set_defaults(TConfig &config)
{
    config.configured = APP_CONFIG_DEFAULT_CONFIGURED;
    config.rotation = APP_CONFIG_DEFAULT_ROTATION;
    config.brightness = APP_CONFIG_DEFAULT_BRIGHTNESS;
    config.split_mode = APP_CONFIG_DEFAULT_SPLIT_MODE;
    config.font_size = APP_CONFIG_DEFAULT_FONT_SIZE;
    config.stations.clear();
}

template <typename TConfig> inline void apply_doc(const JsonDocument &doc, TConfig &config)
{
    config.configured = doc["configured"] | APP_CONFIG_DEFAULT_CONFIGURED;
    config.rotation = static_cast<uint8_t>(doc["rotation"] | APP_CONFIG_DEFAULT_ROTATION);
    config.brightness = static_cast<uint8_t>(doc["brightness"] | APP_CONFIG_DEFAULT_BRIGHTNESS);
    config.split_mode = doc["split_mode"] | APP_CONFIG_DEFAULT_SPLIT_MODE;
    config.font_size = static_cast<uint8_t>(doc["font_size"] | APP_CONFIG_DEFAULT_FONT_SIZE);

    JsonArrayConst stations = doc["stations"].as<JsonArrayConst>();
    if (stations.isNull()) {
        return;
    }

    for (JsonObjectConst station_obj : stations) {
        StationResult station;
        station.id = station_obj["id"] | "";
        station.name = station_obj["name"] | "";

        JsonArrayConst lines = station_obj["lines"].as<JsonArrayConst>();
        if (!lines.isNull()) {
            for (JsonObjectConst line_obj : lines) {
                StationLine line;
                line.name = line_obj["name"] | "";
                line.product = line_obj["product"] | "";
                if (!line.name.empty()) {
                    station.lines.push_back(std::move(line));
                }
            }
        }

        if (!station.id.empty()) {
            if (station.name.empty()) {
                station.name = station.id;
            }
            config.stations.push_back(std::move(station));
        }
    }
}

template <typename TConfig> inline DeserializationError deserialize(const std::string &json, TConfig &config)
{
    JsonDocument doc;
    DeserializationError parse_err = deserializeJson(doc, json);
    if (parse_err) {
        return parse_err;
    }

    apply_doc(doc, config);
    return DeserializationError::Ok;
}

template <typename TConfig> inline void serialize(const TConfig &config, std::string &json)
{
    JsonDocument doc;
    doc["v"] = APP_CONFIG_JSON_VERSION;
    doc["configured"] = config.configured;
    doc["rotation"] = config.rotation;
    doc["brightness"] = config.brightness;
    doc["split_mode"] = config.split_mode;
    doc["font_size"] = config.font_size;

    JsonArray stations = doc["stations"].to<JsonArray>();
    for (const auto &station : config.stations) {
        JsonObject station_obj = stations.add<JsonObject>();
        station_obj["id"] = station.id;
        station_obj["name"] = station.name;

        JsonArray lines = station_obj["lines"].to<JsonArray>();
        for (const auto &line : station.lines) {
            JsonObject line_obj = lines.add<JsonObject>();
            line_obj["name"] = line.name;
            line_obj["product"] = line.product;
        }
    }

    json.clear();
    serializeJson(doc, json);
}

} // namespace app_config_codec
