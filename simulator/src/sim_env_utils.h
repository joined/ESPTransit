#pragma once

#include <cstdlib>
#include <cstring>
#include <ctime>

static inline bool env_flag_enabled(const char *name)
{
    const char *raw = std::getenv(name);
    if (!raw || raw[0] == '\0') {
        return false;
    }

    if (strcmp(raw, "0") == 0 || strcmp(raw, "false") == 0 || strcmp(raw, "False") == 0 || strcmp(raw, "FALSE") == 0 ||
        strcmp(raw, "no") == 0 || strcmp(raw, "NO") == 0 || strcmp(raw, "off") == 0 || strcmp(raw, "OFF") == 0) {
        return false;
    }

    return true;
}

// Returns a frozen local-time timestamp (2025-01-01 12:34:56) when running under
// UI_TEST, otherwise returns the real current time. Use wherever time-dependent
// output needs to be deterministic for golden screenshot tests.
static inline time_t sim_get_reference_time()
{
    if (env_flag_enabled("UI_TEST")) {
        struct tm mock = {};
        mock.tm_hour = 12;
        mock.tm_min = 34;
        mock.tm_sec = 56;
        mock.tm_year = 125; // 2025
        mock.tm_mon = 0;    // January
        mock.tm_mday = 1;
        mock.tm_isdst = -1;
        return mktime(&mock);
    }
    return time(nullptr);
}
