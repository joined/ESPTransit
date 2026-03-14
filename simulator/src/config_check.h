#pragma once

#include "lvgl.h"

#if LV_USE_OS != LV_OS_FREERTOS
#error "LV_USE_OS must be LV_OS_FREERTOS. Please check lv_conf.h"
#endif
