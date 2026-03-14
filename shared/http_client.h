#pragma once

#include "app_constants.h"
#include "esp_err.h"
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string_view>

struct StationLine {
    std::string name;
    std::string product;
};

struct StationResult {
    std::string id;
    std::string name;
    std::vector<StationLine> lines;
};

struct DepartureResult {
    std::string trip_id; // unique trip identifier for reconciliation
    std::string stop_id;
    std::string stop_name;
    std::string line_name;
    std::string line_product;
    std::string direction;
    time_t when_time = 0;  // absolute departure time (0 if cancelled)
    int delay_seconds = 0; // delay in seconds (positive = late, negative = early)
    bool cancelled = false;
    bool scheduled = false; // true if no real-time data (prognosisType is null)
};

// HTTP fetcher task infrastructure
enum HttpRequestType { HTTP_REQUEST_DEPARTURES, HTTP_REQUEST_STATION_SEARCH };

struct HttpRequest {
    HttpRequestType type;
    uint32_t generation; // For discarding stale search results
    union {
        char station_ids[STATION_IDS_BUFFER_SIZE];           // For departures (single ID or comma-separated IDs)
        char search_query[STATION_SEARCH_QUERY_BUFFER_SIZE]; // For station search
    };
};

// HTTP client with reusable connection and static response buffer.
// The returned string_view is invalidated by the next get() call.
class HttpClient {
  public:
    static HttpClient &instance();

    virtual esp_err_t get(const std::string &url, std::string_view &response) = 0;

    virtual ~HttpClient() = default;
    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;

  protected:
    HttpClient() = default;
};

// Search for stations matching query. Returns up to the configured API result limit.
esp_err_t http_search_stations(const char *query, std::vector<StationResult> &results);

// Fetch departures for one or many stations.
esp_err_t http_fetch_departures(const std::vector<std::string> &station_ids, std::vector<DepartureResult> &results);
