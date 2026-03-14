#include "http_fetcher.h"
#include "app_constants.h"
#include "http_parse.h"
#include "http_util.h"
#include "esp_log.h"
#include <cstdlib>
#include <memory>
#include <string>

static const char *TAG = "http_fetcher";

HttpFetcher::HttpFetcher(PostCommandFn post_command, DeparturesCallback on_departures, StationsCallback on_stations)
    : post_command_(std::move(post_command)), on_departures_(std::move(on_departures)),
      on_stations_(std::move(on_stations))
{
}

void HttpFetcher::start()
{
    queue_ = xQueueCreate(HTTP_REQUEST_QUEUE_LENGTH, sizeof(HttpRequest));
    if (!queue_) {
        ESP_LOGE(TAG, "Failed to create HTTP request queue");
        abort();
    }

    xTaskCreate(HttpFetcher::taskEntry, "http_fetcher", 8192, this, 4, &task_handle_);
}

bool HttpFetcher::enqueue(const HttpRequest &request)
{
    if (xQueueSend(queue_, &request, 0) != pdTRUE) {
        ESP_LOGW(TAG, "HTTP request queue full, dropping request type=%d gen=%lu", (int)request.type,
                 (unsigned long)request.generation);
        return false;
    }
    return true;
}

void HttpFetcher::taskEntry(void *arg)
{
    static_cast<HttpFetcher *>(arg)->run();
}

void HttpFetcher::run()
{
    ESP_LOGI(TAG, "HTTP fetcher task started");

    HttpRequest request;
    while (true) {
        if (xQueueReceive(queue_, &request, portMAX_DELAY) == pdTRUE) {
            if (request.type == HTTP_REQUEST_DEPARTURES) {
                fetchDepartures(request.station_ids);
            } else if (request.type == HTTP_REQUEST_STATION_SEARCH) {
                fetchStations(request.search_query, request.generation);
            }
        }
    }
}

void HttpFetcher::fetchDepartures(const char *station_ids)
{
    ESP_LOGI(TAG, "Fetching departures for %s", station_ids);

    std::vector<std::string> stop_ids = extract_stop_ids(station_ids);
    auto departures = std::make_shared<std::vector<DepartureResult>>();
    esp_err_t error = http_fetch_departures(stop_ids, *departures);

    post_command_([this, departures, error] { on_departures_(error, departures); });
}

void HttpFetcher::fetchStations(const char *query, uint32_t generation)
{
    ESP_LOGI(TAG, "Searching stations for '%s'", query);

    auto stations = std::make_shared<std::vector<StationResult>>();
    esp_err_t error = http_search_stations(query, *stations);

    post_command_([this, stations, error, generation] { on_stations_(error, stations, generation); });
}

// === Shared HTTP API implementations ===

esp_err_t http_search_stations(const char *query, std::vector<StationResult> &results)
{
    results.clear();

    const char *safe_query = query ? query : "";
    std::string url = std::string(TRANSIT_API_BASE_URL) + "/locations?query=" + url_encode(safe_query) +
                      "&results=" + std::to_string(STATION_SEARCH_API_RESULTS) +
                      "&stops=true&addresses=false&poi=false&linesOfStops=true";

    std::string_view response;
    esp_err_t err = HttpClient::instance().get(url, response);
    if (err != ESP_OK)
        return err;

    esp_err_t parse_err = parse_station_search_json(response, results);
    if (parse_err != ESP_OK) {
        ESP_LOGE(TAG, "JSON parse error");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Station search '%s': %d results", safe_query, (int)results.size());
    return ESP_OK;
}

esp_err_t http_fetch_departures(const std::vector<std::string> &station_ids, std::vector<DepartureResult> &results)
{
    results.clear();

    if (station_ids.empty()) {
        ESP_LOGE(TAG, "No valid stop IDs provided");
        return ESP_FAIL;
    }

    std::string stop_ids_csv;
    for (const std::string &id : station_ids) {
        if (id.empty())
            continue;
        if (!stop_ids_csv.empty()) {
            stop_ids_csv += ",";
        }
        stop_ids_csv += id;
    }
    if (stop_ids_csv.empty()) {
        ESP_LOGE(TAG, "No valid stop IDs provided");
        return ESP_FAIL;
    }

    std::string url = std::string(TRANSIT_API_BASE_URL) + "/departures?stops=" + url_encode(stop_ids_csv.c_str()) +
                      "&duration=" + std::to_string(DEPARTURES_API_DURATION_MINUTES) +
                      "&results=" + std::to_string(DEPARTURES_API_RESULTS);

    std::string_view response;
    esp_err_t err = HttpClient::instance().get(url, response);
    if (err != ESP_OK)
        return err;

    esp_err_t parse_err = parse_departures_json(response, results);
    if (parse_err != ESP_OK) {
        ESP_LOGE(TAG, "JSON parse error");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Fetched %d departures for stop IDs %s", (int)results.size(), stop_ids_csv.c_str());
    return ESP_OK;
}
