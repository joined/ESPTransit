#include "ui/screens/boot.h"
#include "ui/common.h"
#include "fira_sans.h"

BootScreen::BootScreen()
{
    m_screen = lv_obj_create(nullptr);
    ui_style_screen(m_screen);

    // Center container for logo + status
    lv_obj_t *container = lv_obj_create(m_screen);
    lv_obj_set_size(container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(container);

    // Logo label
    lv_obj_t *logo = lv_label_create(container);
    lv_label_set_text(logo, "ESPTransit");
    lv_obj_set_style_text_font(logo, FONT_LOGO, 0);
    lv_obj_set_style_text_color(logo, UI_COLOR_TEXT, 0);

    // Status label
    m_status_label = lv_label_create(container);
    lv_label_set_text(m_status_label, "");
    lv_obj_set_style_text_font(m_status_label, FONT_BODY, 0);
    lv_obj_set_style_text_color(m_status_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_pad_top(m_status_label, 16, 0);
}

void BootScreen::set_status(const char *text)
{
    lv_label_set_text(m_status_label, text);
}
