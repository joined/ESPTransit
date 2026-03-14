#pragma once

#include <cstdio>
#include <cstdlib>

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_SIZE 0x105

// Simplified ESP_ERROR_CHECK for simulator
#define ESP_ERROR_CHECK(x)                                                                                             \
    do {                                                                                                               \
        esp_err_t err_rc_ = (x);                                                                                       \
        if (err_rc_ != ESP_OK) {                                                                                       \
            fprintf(stderr, "ESP_ERROR_CHECK failed: 0x%x\n", err_rc_);                                                \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)
