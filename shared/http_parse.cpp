#include "http_parse.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"

static struct SpiRamAllocator : ArduinoJson::Allocator {
    void *allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
    void deallocate(void *ptr) override { heap_caps_free(ptr); }
    void *reallocate(void *ptr, size_t new_size) override
    {
        return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
    }
} s_psram_allocator;

struct PsramJsonDocument : JsonDocument {
    PsramJsonDocument() : JsonDocument(&s_psram_allocator) {}
};
#else
using PsramJsonDocument = JsonDocument;
#endif

// Parse ISO 8601 datetime string to time_t
// Format: "2024-01-15T14:30:00+01:00" or with Z
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

static time_t timegm_utc(const struct tm *tm)
{
    const int64_t year = static_cast<int64_t>(tm->tm_year) + 1900;
    const unsigned month = static_cast<unsigned>(tm->tm_mon + 1);
    const unsigned day = static_cast<unsigned>(tm->tm_mday);
    const int64_t days = days_from_civil(year, month, day);
    const int64_t seconds = days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    return static_cast<time_t>(seconds);
}

static time_t parse_iso8601(const char *str)
{
    if (!str)
        return 0;

    struct tm tm = {};
    int year, mon, day, hour, min, sec;
    int tz_h = 0, tz_m = 0;
    char tz_sign = '+';

    bool has_timezone = false;
    int tz_offset_sec = 0;
    int n = sscanf(str, "%d-%d-%dT%d:%d:%d%c%d:%d", &year, &mon, &day, &hour, &min, &sec, &tz_sign, &tz_h, &tz_m);
    if (n == 9) {
        if (tz_sign != '+' && tz_sign != '-') {
            return 0;
        }
        has_timezone = true;
        tz_offset_sec = (tz_h * 3600) + (tz_m * 60);
        if (tz_sign == '-') {
            tz_offset_sec = -tz_offset_sec;
        }
    } else {
        char zulu = '\0';
        n = sscanf(str, "%d-%d-%dT%d:%d:%d%c", &year, &mon, &day, &hour, &min, &sec, &zulu);
        if (n == 7 && zulu == 'Z') {
            has_timezone = true;
            tz_offset_sec = 0;
        } else if (n != 6) {
            return 0;
        }
    }

    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;

    if (has_timezone) {
        time_t utc = timegm_utc(&tm);
        return utc - tz_offset_sec;
    }

    return mktime(&tm);
}

esp_err_t parse_station_search_json(std::string_view json, std::vector<StationResult> &results)
{
    if (json.empty())
        return ESP_FAIL;

    PsramJsonDocument doc;

    DeserializationError json_err = deserializeJson(doc, json.data(), json.size());
    if (json_err)
        return ESP_FAIL;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject loc : arr) {
        const char *type = loc["type"] | "";
        if (strcmp(type, "stop") != 0 && strcmp(type, "station") != 0)
            continue;

        StationResult sr;
        sr.id = loc["id"] | "";
        sr.name = loc["name"] | "";

        JsonArray lines = loc["lines"].as<JsonArray>();
        if (!lines.isNull()) {
            for (JsonObject line : lines) {
                const char *ln = line["name"] | "";
                const char *lp = line["product"] | "";
                if (strlen(ln) > 0) {
                    sr.lines.push_back({ln, lp});
                }
            }
        }

        if (!sr.id.empty() && !sr.name.empty()) {
            results.push_back(std::move(sr));
        }
    }

    return ESP_OK;
}

esp_err_t parse_departures_json(std::string_view json, std::vector<DepartureResult> &results)
{
    if (json.empty())
        return ESP_FAIL;

    PsramJsonDocument doc;

    DeserializationError json_err = deserializeJson(doc, json.data(), json.size());
    if (json_err)
        return ESP_FAIL;

    JsonArray departures = doc["departures"].as<JsonArray>();
    if (departures.isNull())
        return ESP_OK;

    for (JsonObject dep : departures) {
        DepartureResult dr;
        dr.trip_id = dep["tripId"] | "";

        JsonObject stop = dep["stop"].as<JsonObject>();
        if (!stop.isNull()) {
            dr.stop_id = stop["id"] | "";
            dr.stop_name = stop["name"] | "";
        }

        JsonObject line = dep["line"].as<JsonObject>();
        if (!line.isNull()) {
            dr.line_name = line["name"] | "";
            dr.line_product = line["product"] | "";
        }

        dr.direction = dep["direction"] | "";
        dr.cancelled = dep["cancelled"] | false;

        const char *when_str = dep["when"] | (const char *)nullptr;
        if (dr.cancelled || !when_str) {
            dr.when_time = 0;
        } else {
            dr.when_time = parse_iso8601(when_str);
        }

        dr.delay_seconds = dep["delay"] | 0;
        dr.scheduled = dep["prognosisType"].isNull();

        results.push_back(std::move(dr));
    }

    return ESP_OK;
}
