#include "sim_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdarg.h>
#include <chrono>
#include <ctime>
#include <cstring>

void sim_log_print_time()
{
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);

    long long millis = value.count();
    long long seconds = millis / 1000;
    long long ms = millis % 1000;

    time_t sec = seconds;
    struct tm timeinfo;
    localtime_r(&sec, &timeinfo);

    printf("%02d:%02d:%02d.%03lld ", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, ms);
}

void sim_log_print(sim_log_level_t level, const char *tag, const char *format, ...)
{
    sim_log_print_time();

    // Print level prefix
    switch (level) {
    case SIM_LOG_ERROR:
        printf("[ERROR]");
        break;
    case SIM_LOG_WARN:
        printf("[WARN]");
        break;
    case SIM_LOG_DEBUG:
        printf("[DEBUG]");
        break;
    case SIM_LOG_INFO:
        // No prefix for INFO level
        break;
    }

    // Print tag
    printf("[%s] ", tag);

    // Print formatted message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

// LVGL log callback
static void lvgl_log_callback(lv_log_level_t level, const char *buf)
{
    (void)level; // Unused - LVGL includes level in the buffer

    sim_log_print_time();

    printf("[lvgl] %.*s", (int)strlen(buf), buf);
    fflush(stdout);
}

void sim_log_init_lvgl()
{
    lv_log_register_print_cb(lvgl_log_callback);
}
