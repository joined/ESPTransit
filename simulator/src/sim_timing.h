#pragma once

#include <stdint.h>

// Returns parsed delay scale from SIMULATOR_DELAY_SCALE (default 1.0).
float sim_delay_scale();

// Delay mock timing by base_ms scaled with SIMULATOR_DELAY_SCALE.
void sim_delay_ms(uint32_t base_ms);
