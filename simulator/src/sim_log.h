#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SIM_LOG_ERROR, SIM_LOG_WARN, SIM_LOG_INFO, SIM_LOG_DEBUG } sim_log_level_t;

void sim_log_print(sim_log_level_t level, const char *tag, const char *format, ...);
void sim_log_print_time();
void sim_log_init_lvgl();

#ifdef __cplusplus
}
#endif
