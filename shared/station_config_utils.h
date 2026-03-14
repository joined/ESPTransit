#pragma once

#include "http_client.h"
#include <string>
#include <vector>

bool normalize_station(const StationResult &station_in, StationResult &station_out);
std::vector<StationResult> normalize_station_list(const std::vector<StationResult> &stations);
bool station_lists_equal(const std::vector<StationResult> &a, const std::vector<StationResult> &b);
std::string build_station_ids_csv(const std::vector<StationResult> &stations);
