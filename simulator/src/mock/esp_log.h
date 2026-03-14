#pragma once
#include "esp_err.h"
#include "sim_log.h"

#define ESP_LOGI(tag, format, ...) sim_log_print(SIM_LOG_INFO, tag, format, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) sim_log_print(SIM_LOG_ERROR, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) sim_log_print(SIM_LOG_WARN, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) sim_log_print(SIM_LOG_DEBUG, tag, format, ##__VA_ARGS__)
