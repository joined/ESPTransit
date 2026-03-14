#include "station_badges.h"
#include "ui/common.h"
#include "fira_sans.h"
#include <string>
#include <unordered_set>

lv_obj_t *ui_create_station_badges(lv_obj_t *parent, const std::vector<StationLine> &lines, lv_coord_t width,
                                   bool dedupe, const std::string &fallback_text)
{
    lv_obj_t *badges = lv_obj_create(parent);
    lv_obj_remove_style_all(badges);
    lv_obj_set_width(badges, width);
    lv_obj_set_height(badges, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(badges, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(badges, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(badges, 4, 0);
    lv_obj_clear_flag(badges, LV_OBJ_FLAG_CLICKABLE);

    std::unordered_set<std::string> seen;
    size_t rendered = 0;
    for (const auto &line : lines) {
        if (line.name.empty()) {
            continue;
        }

        if (dedupe) {
            std::string key = line.name;
            key += "\x1F";
            key += line.product;
            if (!seen.insert(key).second) {
                continue;
            }
        }

        lv_obj_t *badge = lv_obj_create(badges);
        lv_obj_remove_style_all(badge);
        lv_obj_set_size(badge, LV_SIZE_CONTENT, 24);
        lv_obj_set_style_pad_left(badge, 6, 0);
        lv_obj_set_style_pad_right(badge, 6, 0);
        lv_obj_set_style_bg_color(badge, ui_product_color(line.product), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, 4, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label = lv_label_create(badge);
        lv_label_set_text(label, line.name.c_str());
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(label, FONT_SMALL, 0);
        lv_obj_center(label);
        rendered++;
    }

    if (rendered == 0 && !fallback_text.empty()) {
        lv_obj_t *fallback = lv_label_create(badges);
        lv_label_set_text(fallback, fallback_text.c_str());
        lv_obj_set_style_text_color(fallback, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(fallback, FONT_SMALL, 0);
        lv_obj_clear_flag(fallback, LV_OBJ_FLAG_CLICKABLE);
    }

    return badges;
}
