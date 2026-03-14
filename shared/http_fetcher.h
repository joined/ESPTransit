#pragma once

#include "http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <functional>
#include <memory>
#include <vector>

class HttpFetcher {
  public:
    using PostCommandFn = std::function<void(std::function<void()>)>;
    using DeparturesCallback = std::function<void(esp_err_t, std::shared_ptr<std::vector<DepartureResult>>)>;
    using StationsCallback = std::function<void(esp_err_t, std::shared_ptr<std::vector<StationResult>>, uint32_t gen)>;

    HttpFetcher(PostCommandFn post_command, DeparturesCallback on_departures, StationsCallback on_stations);

    void start();
    bool enqueue(const HttpRequest &request);

    static void taskEntry(void *arg);

  private:
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    PostCommandFn post_command_;
    DeparturesCallback on_departures_;
    StationsCallback on_stations_;

    void run();
    void fetchDepartures(const char *station_ids);
    void fetchStations(const char *query, uint32_t generation);
};
