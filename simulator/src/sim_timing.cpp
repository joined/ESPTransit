#include "sim_timing.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cmath>
#include <cstdlib>

static float parse_delay_scale()
{
    const char *raw = std::getenv("SIMULATOR_DELAY_SCALE");
    if (!raw || *raw == '\0') {
        return 1.0f;
    }

    char *end = nullptr;
    double parsed = std::strtod(raw, &end);
    if (end == raw || *end != '\0' || !std::isfinite(parsed) || parsed < 0.0) {
        return 1.0f;
    }

    return static_cast<float>(parsed);
}

float sim_delay_scale()
{
    // Parse once so runtime delay calls avoid repeated env/strtod overhead.
    static float scale = parse_delay_scale();
    return scale;
}

void sim_delay_ms(uint32_t base_ms)
{
    if (base_ms == 0) {
        return;
    }

    float scale = sim_delay_scale();
    uint32_t scaled_ms = static_cast<uint32_t>(base_ms * scale + 0.5f);

    // Keep ordering effects when scale is tiny but non-zero.
    if (scale > 0.0f && scaled_ms == 0) {
        scaled_ms = 1;
    }
    if (scaled_ms == 0) {
        return;
    }

    TickType_t ticks = pdMS_TO_TICKS(scaled_ms);
    if (ticks == 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
}
