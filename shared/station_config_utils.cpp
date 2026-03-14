#include "station_config_utils.h"
#include "app_constants.h"
#include "esp_log.h"
#include <algorithm>
#include <unordered_set>

namespace {

static const char *TAG = "station_config";

std::string trim_station_name(const std::string &name, const std::string &fallback_id)
{
    if (name.empty()) {
        return fallback_id;
    }
    std::string trimmed = name;
    if (trimmed.size() > MAX_STATION_NAME_LENGTH) {
        trimmed.resize(MAX_STATION_NAME_LENGTH);
    }
    return trimmed;
}

std::vector<StationLine> dedupe_station_lines(const std::vector<StationLine> &lines)
{
    std::vector<StationLine> out;
    out.reserve(lines.size());

    std::unordered_set<std::string> seen;
    for (const auto &line : lines) {
        if (line.name.empty()) {
            continue;
        }
        std::string key = line.name + "\x1F" + line.product;
        if (!seen.insert(key).second) {
            continue;
        }
        out.push_back(line);
    }

    return out;
}

} // namespace

bool normalize_station(const StationResult &station_in, StationResult &station_out)
{
    std::string id = station_in.id;
    if (id.empty() || id.size() > MAX_STATION_ID_LENGTH) {
        return false;
    }

    station_out = station_in;
    station_out.id = id;
    station_out.name = trim_station_name(station_in.name, id);
    station_out.lines = dedupe_station_lines(station_in.lines);
    return true;
}

std::vector<StationResult> normalize_station_list(const std::vector<StationResult> &stations)
{
    std::vector<StationResult> out;
    out.reserve(std::min(stations.size(), MAX_CONFIGURED_STATIONS));

    std::unordered_set<std::string> seen_ids;
    for (const auto &station : stations) {
        if (out.size() >= MAX_CONFIGURED_STATIONS) {
            ESP_LOGW(TAG, "Configured stations exceed max (%d); keeping first %d station(s)",
                     (int)MAX_CONFIGURED_STATIONS, (int)MAX_CONFIGURED_STATIONS);
            break;
        }

        StationResult normalized;
        if (!normalize_station(station, normalized)) {
            continue;
        }

        if (!seen_ids.insert(normalized.id).second) {
            continue;
        }
        out.push_back(std::move(normalized));
    }

    return out;
}

bool station_lists_equal(const std::vector<StationResult> &a, const std::vector<StationResult> &b)
{
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].name != b[i].name) {
            return false;
        }
        if (a[i].lines.size() != b[i].lines.size()) {
            return false;
        }
        for (size_t j = 0; j < a[i].lines.size(); ++j) {
            if (a[i].lines[j].name != b[i].lines[j].name || a[i].lines[j].product != b[i].lines[j].product) {
                return false;
            }
        }
    }

    return true;
}

std::string build_station_ids_csv(const std::vector<StationResult> &stations)
{
    std::string ids_csv;
    for (const auto &station : stations) {
        if (station.id.empty()) {
            continue;
        }
        if (!ids_csv.empty()) {
            ids_csv += ",";
        }
        ids_csv += station.id;
    }
    return ids_csv;
}
