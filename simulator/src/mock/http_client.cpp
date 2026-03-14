#include "http_client.h"
#include "sim_env_utils.h"
#include "sim_log.h"
#include "sim_timing.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <pthread.h>
#include <ArduinoJson.h>

#include <curl/curl.h>
#include "FreeRTOS.h"
#include "task.h"

static const char *TAG = "http";

// Mock mode flag (set by main.cpp)
extern bool g_mock_http;

// === Native pthread worker for non-blocking curl ===
// This runs curl outside of FreeRTOS so it doesn't block the scheduler.

static pthread_t s_curl_thread;
static pthread_mutex_t s_curl_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_curl_cond = PTHREAD_COND_INITIALIZER;
static bool s_curl_thread_running = false;

static struct {
    std::string url;
    std::string body;
    esp_err_t result;
    bool request_pending;
    bool done;
} s_curl_work;

static size_t curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    if (!userdata || !ptr)
        return 0;
    std::string *out = static_cast<std::string *>(userdata);
    size_t total = size * nmemb;
    out->append(ptr, total);
    return total;
}

// Perform the actual curl request (called from worker thread)
static esp_err_t do_curl_request(const std::string &url, std::string &out_body)
{
    if (url.empty())
        return ESP_FAIL;

    CURL *curl = curl_easy_init();
    if (!curl) {
        sim_log_print(SIM_LOG_ERROR, TAG, "curl_easy_init failed");
        return ESP_FAIL;
    }

    out_body.clear();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_body);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        sim_log_print(SIM_LOG_ERROR, TAG, "curl_easy_perform failed: %s", curl_easy_strerror(res));
        return ESP_FAIL;
    }
    if (status != 200) {
        sim_log_print(SIM_LOG_ERROR, TAG, "HTTP status %ld", status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Worker thread that processes curl requests
static void *curl_worker_thread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&s_curl_mutex);

    while (s_curl_thread_running) {
        // Wait for work
        while (!s_curl_work.request_pending && s_curl_thread_running) {
            pthread_cond_wait(&s_curl_cond, &s_curl_mutex);
        }

        if (!s_curl_thread_running)
            break;

        // Copy URL and release mutex during curl work
        std::string url = s_curl_work.url;
        pthread_mutex_unlock(&s_curl_mutex);

        // Do curl (outside mutex so other threads can check status)
        std::string body;
        esp_err_t err = do_curl_request(url, body);

        // Store result
        pthread_mutex_lock(&s_curl_mutex);
        s_curl_work.body = std::move(body);
        s_curl_work.result = err;
        s_curl_work.done = true;
        s_curl_work.request_pending = false;
    }

    pthread_mutex_unlock(&s_curl_mutex);
    return nullptr;
}

static void ensure_curl_thread_started()
{
    pthread_mutex_lock(&s_curl_mutex);
    if (!s_curl_thread_running) {
        s_curl_thread_running = true;
        s_curl_work.request_pending = false;
        s_curl_work.done = false;
        pthread_create(&s_curl_thread, nullptr, curl_worker_thread, nullptr);
    }
    pthread_mutex_unlock(&s_curl_mutex);
}

// Non-blocking HTTP GET that polls with vTaskDelay (allows FreeRTOS tasks to run)
static esp_err_t curl_get(const char *url, std::string &out_body)
{
    if (!url || !*url)
        return ESP_FAIL;

    ensure_curl_thread_started();

    // Submit request to worker thread
    pthread_mutex_lock(&s_curl_mutex);
    s_curl_work.url = url;
    s_curl_work.body.clear();
    s_curl_work.done = false;
    s_curl_work.request_pending = true;
    pthread_cond_signal(&s_curl_cond);
    pthread_mutex_unlock(&s_curl_mutex);

    // Poll for completion, yielding to other FreeRTOS tasks
    while (true) {
        pthread_mutex_lock(&s_curl_mutex);
        bool done = s_curl_work.done;
        pthread_mutex_unlock(&s_curl_mutex);

        if (done)
            break;
        vTaskDelay(pdMS_TO_TICKS(5)); // Yield to LVGL and other tasks
    }

    // Get result
    pthread_mutex_lock(&s_curl_mutex);
    out_body = std::move(s_curl_work.body);
    esp_err_t err = s_curl_work.result;
    pthread_mutex_unlock(&s_curl_mutex);

    return err;
}

// === Mock JSON responses ===

static std::string make_mock_stations_json()
{
    struct MockStation {
        const char *id;
        const char *name;
        struct {
            const char *name;
            const char *product;
        } lines[4];
        int line_count;
    };

    const MockStation stations[] = {
        {"900000003201", "Berlin Hauptbahnhof", {{"S5", "suburban"}, {"S7", "suburban"}, {"U5", "subway"}}, 3},
        {"900000120003", "Berlin Ostbahnhof", {{"S3", "suburban"}, {"S5", "suburban"}, {"S7", "suburban"}}, 3},
        {"900000024101", "Berlin Alexanderplatz", {{"U2", "subway"}, {"U5", "subway"}, {"U8", "subway"}}, 3},
        {"900000100001", "Berlin Friedrichstraße", {{"S1", "suburban"}, {"S2", "suburban"}, {"U6", "subway"}}, 3},
        {"900000100003", "Berlin Zoologischer Garten", {{"S5", "suburban"}, {"S7", "suburban"}, {"U2", "subway"}}, 3},
        {"900000058101", "Berlin Südkreuz", {{"S2", "suburban"}, {"S25", "suburban"}, {"RE1", "regional"}}, 3},
        {"900000078101", "Berlin Gesundbrunnen", {{"S1", "suburban"}, {"U8", "subway"}}, 2},
        {"900000160004", "Berlin Warschauer Straße", {{"S3", "suburban"}, {"U1", "subway"}, {"M10", "tram"}}, 3},
        {"900000009101", "Berlin Jungfernheide", {{"S41", "suburban"}, {"S42", "suburban"}, {"U7", "subway"}}, 3},
        {"900000023201", "Berlin Potsdamer Platz", {{"S1", "suburban"}, {"S2", "suburban"}, {"U2", "subway"}}, 3},
    };

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto &s : stations) {
        JsonObject stop = arr.add<JsonObject>();
        stop["type"] = "stop";
        stop["id"] = s.id;
        stop["name"] = s.name;
        JsonArray lines = stop["lines"].to<JsonArray>();
        for (int i = 0; i < s.line_count; i++) {
            JsonObject line = lines.add<JsonObject>();
            line["name"] = s.lines[i].name;
            line["product"] = s.lines[i].product;
        }
    }

    std::string json;
    serializeJson(doc, json);
    return json;
}

// Format time_t as ISO 8601 UTC time string.
static std::string iso8601(time_t t)
{
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+00:00", &tm);
    return buf;
}

static std::string make_mock_departures_json()
{
    time_t now = sim_get_reference_time();

    struct MockDep {
        const char *trip_id;
        const char *stop_id;
        const char *stop_name;
        const char *line;
        const char *product;
        const char *direction;
        int when_offset; // 0 means null (for cancelled)
        int planned_offset;
        int delay;
        bool cancelled;
        bool has_prognosis;
    };

    const MockDep deps[] = {
        {"900000003201_trip_0", "900000003201", "Berlin Hauptbahnhof", "S1", "suburban", "Wannsee", 120, 120, 0, false,
         true},
        {"900000003201_trip_1", "900000003201", "Berlin Hauptbahnhof", "S2", "suburban", "Blankenfelde", 360, 240, 120,
         false, true},
        {"900000003201_trip_2", "900000003201", "Berlin Hauptbahnhof", "U6", "subway", "Alt-Mariendorf", 300, 300, 0,
         false, true},
        {"900000120003_trip_0", "900000120003", "Berlin Ostbahnhof", "M10", "tram", "Warschauer Str.", 360, 420, -60,
         false, true},
        {"900000120003_trip_1", "900000120003", "Berlin Ostbahnhof", "S5", "suburban", "Strausberg Nord", 540, 540, 0,
         false, true},
        {"900000120003_trip_2", "900000120003", "Berlin Ostbahnhof", "S7", "suburban", "Ahrensfelde", 720, 720, 0,
         false, true},
        {"900000024101_trip_0", "900000024101", "Berlin Alexanderplatz", "RE1", "regional", "Brandenburg", 0, 900, 0,
         true, false},
        {"900000024101_trip_1", "900000024101", "Berlin Alexanderplatz", "RB14", "regional", "Nauen", 1080, 1080, 0,
         false, true},
        {"900000024101_trip_2", "900000024101", "Berlin Alexanderplatz", "U2", "subway", "Pankow", 480, 480, 0, false,
         true},
        {"900000024101_trip_3", "900000024101", "Berlin Alexanderplatz", "100", "bus", "Zoologischer Garten", 180, 180,
         0, false, true},
        {"900000100001_trip_0", "900000100001", "Berlin Friedrichstraße", "S1", "suburban", "Oranienburg", 240, 240, 0,
         false, true},
        {"900000100001_trip_1", "900000100001", "Berlin Friedrichstraße", "U6", "subway", "Alt-Tegel", 420, 360, 60,
         false, true},
        {"900000100001_trip_2", "900000100001", "Berlin Friedrichstraße", "S2", "suburban", "Blankenfelde", 600, 600, 0,
         false, true},
    };

    JsonDocument doc;
    JsonArray departures = doc["departures"].to<JsonArray>();

    for (const auto &d : deps) {
        JsonObject dep = departures.add<JsonObject>();
        dep["tripId"] = d.trip_id;
        JsonObject stop = dep["stop"].to<JsonObject>();
        stop["id"] = d.stop_id;
        stop["name"] = d.stop_name;
        JsonObject line = dep["line"].to<JsonObject>();
        line["name"] = d.line;
        line["product"] = d.product;
        dep["direction"] = d.direction;
        if (!d.cancelled)
            dep["when"] = iso8601(now + d.when_offset);
        dep["plannedWhen"] = iso8601(now + d.planned_offset);
        if (!d.cancelled)
            dep["delay"] = d.delay;
        dep["cancelled"] = d.cancelled;
        if (d.has_prognosis)
            dep["prognosisType"] = "prognosed";
    }

    std::string json;
    serializeJson(doc, json);
    return json;
}

// === Simulator HTTP client ===

class SimHttpClient : public HttpClient {
  public:
    esp_err_t get(const std::string &url, std::string_view &response) override
    {
        response = {};

        std::string body;

        if (g_mock_http) {
            if (url.find("/locations?") != std::string::npos) {
                sim_log_print(SIM_LOG_INFO, TAG, "[MOCK] Station search request");
                sim_delay_ms(500);
                body = make_mock_stations_json();
            } else if (url.find("/departures?") != std::string::npos) {
                sim_log_print(SIM_LOG_INFO, TAG, "[MOCK] Departures request");
                sim_delay_ms(800);
                body = make_mock_departures_json();
            } else {
                sim_log_print(SIM_LOG_WARN, TAG, "[MOCK] Unknown URL pattern: %s", url.c_str());
                return ESP_FAIL;
            }
        } else {
            esp_err_t err = curl_get(url.c_str(), body);
            if (err != ESP_OK)
                return err;
        }

        if (body.size() + 1 > HTTP_RESPONSE_BUFFER_SIZE) {
            sim_log_print(SIM_LOG_ERROR, TAG, "Response too large (%zu bytes, max %zu)", body.size(),
                          HTTP_RESPONSE_BUFFER_SIZE);
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(buffer_, body.data(), body.size());
        buffer_[body.size()] = '\0';

        response = std::string_view(buffer_, body.size());
        return ESP_OK;
    }

  private:
    char buffer_[HTTP_RESPONSE_BUFFER_SIZE];
};

HttpClient &HttpClient::instance()
{
    static SimHttpClient instance;
    return instance;
}
