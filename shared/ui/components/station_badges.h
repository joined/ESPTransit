#pragma once

#include "http_client.h"
#include "lvgl.h"
#include <string>
#include <vector>

// Create a wrapped badge container and populate it with station lines.
// When no badge is rendered and fallback_text is non-null, a muted fallback label is shown.
lv_obj_t *ui_create_station_badges(lv_obj_t *parent, const std::vector<StationLine> &lines, lv_coord_t width,
                                   bool dedupe, const std::string &fallback_text = {});
