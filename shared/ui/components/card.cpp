#include "card.h"
#include "ui/common.h"

lv_obj_t *ui_create_card(lv_obj_t *parent, int32_t pad_all, int32_t pad_gap, int32_t radius)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, pad_all, 0);
    lv_obj_set_style_pad_gap(card, pad_gap, 0);
    lv_obj_set_style_bg_color(card, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, radius, 0);
    return card;
}
