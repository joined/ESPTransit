#pragma once

#include "esp_err.h"
#include "http_client.h"
#include <string_view>

esp_err_t parse_station_search_json(std::string_view json, std::vector<StationResult> &results);
esp_err_t parse_departures_json(std::string_view json, std::vector<DepartureResult> &results);
