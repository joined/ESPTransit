#include "ui/common.h"
#include "app_platform.h"
#include "bsp/display.h"
#include "station_products.h"
#include "fira_sans.h"
#include <string>
#include <unordered_map>

#if !defined(LV_USE_OBJ_NAME) || (LV_USE_OBJ_NAME != 1)
#error "LV_USE_OBJ_NAME must be set to 1 for stable UI test selectors."
#endif

// ===== Font Size Preset System =====

struct FontPreset {
    const lv_font_t *title;
    const lv_font_t *title_bold;
    const lv_font_t *body;
    const lv_font_t *body_bold;
    const lv_font_t *button;
    const lv_font_t *input;
    const lv_font_t *medium;
    const lv_font_t *small;
    const lv_font_t *logo;
    const lv_font_t *settings_title;
    const lv_font_t *settings_section_title;
    const lv_font_t *departures_split_title;
};

static const FontPreset s_font_presets[] = {
    // SMALL (10.1" board, current defaults)
    {
        .title = &fira_sans_24,
        .title_bold = &fira_sans_24_bold,
        .body = &fira_sans_20,
        .body_bold = &fira_sans_20_bold,
        .button = &fira_sans_20,
        .input = &fira_sans_24,
        .medium = &fira_sans_16,
        .small = &fira_sans_14,
        .logo = &fira_sans_48_bold,
        .settings_title = &fira_sans_28,
        .settings_section_title = &fira_sans_22,
        .departures_split_title = &fira_sans_22,
    },
    // MEDIUM (7" board)
    {
        .title = &fira_sans_24,
        .title_bold = &fira_sans_24_bold,
        .body = &fira_sans_20,
        .body_bold = &fira_sans_20_bold,
        .button = &fira_sans_20,
        .input = &fira_sans_24,
        .medium = &fira_sans_18,
        .small = &fira_sans_16,
        .logo = &fira_sans_48_bold,
        .settings_title = &fira_sans_28,
        .settings_section_title = &fira_sans_22,
        .departures_split_title = &fira_sans_22,
    },
    // LARGE (4.3" board)
    {
        .title = &fira_sans_28,
        .title_bold = &fira_sans_28_bold,
        .body = &fira_sans_24,
        .body_bold = &fira_sans_24_bold,
        .button = &fira_sans_24,
        .input = &fira_sans_28,
        .medium = &fira_sans_20,
        .small = &fira_sans_18,
        .logo = &fira_sans_56_bold,
        .settings_title = &fira_sans_28_bold,
        .settings_section_title = &fira_sans_24,
        .departures_split_title = &fira_sans_24,
    },
};

static FontSize s_current_font_size = FontSize::SMALL;

void ui_set_font_size(FontSize size)
{
    s_current_font_size = size;
}

FontSize ui_get_font_size()
{
    return s_current_font_size;
}

static const FontPreset &current_preset()
{
    return s_font_presets[static_cast<uint8_t>(s_current_font_size)];
}

const lv_font_t *ui_font_title()
{
    return current_preset().title;
}
const lv_font_t *ui_font_title_bold()
{
    return current_preset().title_bold;
}
const lv_font_t *ui_font_body()
{
    return current_preset().body;
}
const lv_font_t *ui_font_body_bold()
{
    return current_preset().body_bold;
}
const lv_font_t *ui_font_button()
{
    return current_preset().button;
}
const lv_font_t *ui_font_input()
{
    return current_preset().input;
}
const lv_font_t *ui_font_medium()
{
    return current_preset().medium;
}
const lv_font_t *ui_font_small()
{
    return current_preset().small;
}
const lv_font_t *ui_font_logo()
{
    return current_preset().logo;
}
const lv_font_t *ui_font_settings_title()
{
    return current_preset().settings_title;
}
const lv_font_t *ui_font_settings_section_title()
{
    return current_preset().settings_section_title;
}
const lv_font_t *ui_font_departures_split_title()
{
    return current_preset().departures_split_title;
}

// ===== Glyph Width Cache =====

namespace {

struct GlyphWidthCacheKey {
    const lv_font_t *font;
    uint32_t letter;
    uint32_t letter_next;

    bool operator==(const GlyphWidthCacheKey &other) const
    {
        return font == other.font && letter == other.letter && letter_next == other.letter_next;
    }
};

struct GlyphWidthCacheKeyHash {
    size_t operator()(const GlyphWidthCacheKey &key) const
    {
        size_t h = reinterpret_cast<size_t>(key.font);
        h ^= static_cast<size_t>(key.letter) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
        h ^= static_cast<size_t>(key.letter_next) + 0x9e3779b97f4a7c15ULL + (h << 6U) + (h >> 2U);
        return h;
    }
};

std::unordered_map<GlyphWidthCacheKey, int32_t, GlyphWidthCacheKeyHash> s_glyph_width_cache;

uint32_t decode_utf8_codepoint(const char *text, size_t len, size_t &index)
{
    if (!text || index >= len) {
        return 0;
    }

    auto is_continuation = [](uint8_t c) -> bool { return (c & 0xC0) == 0x80; };

    size_t start = index;
    uint8_t c0 = static_cast<uint8_t>(text[start]);

    if (c0 < 0x80) {
        index = start + 1;
        return c0;
    }

    if ((c0 & 0xE0) == 0xC0 && start + 1 < len) {
        uint8_t c1 = static_cast<uint8_t>(text[start + 1]);
        if (is_continuation(c1)) {
            uint32_t cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            if (cp >= 0x80) {
                index = start + 2;
                return cp;
            }
        }
    } else if ((c0 & 0xF0) == 0xE0 && start + 2 < len) {
        uint8_t c1 = static_cast<uint8_t>(text[start + 1]);
        uint8_t c2 = static_cast<uint8_t>(text[start + 2]);
        if (is_continuation(c1) && is_continuation(c2)) {
            uint32_t cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            if (cp >= 0x800 && !(cp >= 0xD800 && cp <= 0xDFFF)) {
                index = start + 3;
                return cp;
            }
        }
    } else if ((c0 & 0xF8) == 0xF0 && start + 3 < len) {
        uint8_t c1 = static_cast<uint8_t>(text[start + 1]);
        uint8_t c2 = static_cast<uint8_t>(text[start + 2]);
        uint8_t c3 = static_cast<uint8_t>(text[start + 3]);
        if (is_continuation(c1) && is_continuation(c2) && is_continuation(c3)) {
            uint32_t cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            if (cp >= 0x10000 && cp <= 0x10FFFF) {
                index = start + 4;
                return cp;
            }
        }
    }

    // Invalid byte sequence. Consume one byte to avoid getting stuck.
    index = start + 1;
    return 0xFFFD;
}

int32_t cached_glyph_width(const lv_font_t *font, uint32_t letter, uint32_t letter_next)
{
    if (!font) {
        return 0;
    }

    GlyphWidthCacheKey key{font, letter, letter_next};
    auto it = s_glyph_width_cache.find(key);
    if (it != s_glyph_width_cache.end()) {
        return it->second;
    }

    int32_t width = static_cast<int32_t>(lv_font_get_glyph_width(font, letter, letter_next));
    s_glyph_width_cache.emplace(key, width);
    return width;
}

} // namespace

int32_t ui_text_width_px(const lv_font_t *font, const std::string &text)
{
    if (!font || text.empty()) {
        return 0;
    }

    const char *data = text.data();
    size_t len = text.size();
    size_t index = 0;
    int32_t width = 0;

    while (index < len) {
        size_t next_index = index;
        uint32_t letter = decode_utf8_codepoint(data, len, next_index);

        uint32_t letter_next = 0;
        if (next_index < len) {
            size_t lookahead = next_index;
            letter_next = decode_utf8_codepoint(data, len, lookahead);
        }

        width += cached_glyph_width(font, letter, letter_next);
        index = next_index;
    }

    return width;
}

size_t ui_max_repeated_text_count_for_width_px(const lv_font_t *font, const std::string &sample_text,
                                               int32_t max_width_px)
{
    if (!font || sample_text.empty() || max_width_px <= 0) {
        return 0;
    }

    int32_t sample_width = ui_text_width_px(font, sample_text);
    if (sample_width <= 0) {
        return 0;
    }

    return static_cast<size_t>(max_width_px / sample_width);
}

lv_obj_t *ui_textarea_create(lv_obj_t *parent)
{
    lv_obj_t *textarea = lv_textarea_create(parent);
    ui_stabilize_textarea_for_tests(textarea);
    return textarea;
}

void ui_stabilize_textarea_for_tests(lv_obj_t *textarea)
{
    if (!textarea || !AppPlatform::is_ui_test_mode()) {
        return;
    }

    // Disable blink animation and hide caret visuals in screenshot test mode.
    // Apply to default/focused/edited selectors because textarea cursor style is stateful.
    const lv_style_selector_t cursor_part = static_cast<lv_style_selector_t>(LV_PART_CURSOR);
    const lv_style_selector_t cursor_selectors[] = {
        cursor_part,
        static_cast<lv_style_selector_t>(cursor_part | static_cast<lv_style_selector_t>(LV_STATE_FOCUSED)),
        static_cast<lv_style_selector_t>(cursor_part | static_cast<lv_style_selector_t>(LV_STATE_EDITED)),
        static_cast<lv_style_selector_t>(cursor_part | static_cast<lv_style_selector_t>(LV_STATE_FOCUSED) |
                                         static_cast<lv_style_selector_t>(LV_STATE_EDITED)),
    };
    for (lv_style_selector_t selector : cursor_selectors) {
        lv_obj_set_style_anim_duration(textarea, 0, selector);
        lv_obj_set_style_bg_opa(textarea, LV_OPA_TRANSP, selector);
        lv_obj_set_style_border_width(textarea, 0, selector);
        lv_obj_set_style_border_opa(textarea, LV_OPA_TRANSP, selector);
        lv_obj_set_style_outline_width(textarea, 0, selector);
        lv_obj_set_style_text_opa(textarea, LV_OPA_TRANSP, selector);
    }
}

bool ui_is_landscape()
{
    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        return false;
    }

    int32_t h_res = lv_display_get_horizontal_resolution(disp);
    int32_t v_res = lv_display_get_vertical_resolution(disp);
    return h_res > 0 && v_res > 0 && h_res > v_res;
}

bool ui_is_native_landscape()
{
    return bsp_display_is_native_landscape();
}

lv_obj_t *ui_btn_create(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
    return btn;
}

static void msgbox_close_cb(lv_event_t *e)
{
    lv_obj_t *msgbox = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    lv_msgbox_close(msgbox);
}

// Helper to style a msgbox button (primary or secondary)
static void style_msgbox_button(lv_obj_t *btn, bool is_primary)
{
    if (is_primary) {
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, UI_COLOR_TEXT, 0);
    } else {
        lv_obj_set_style_bg_color(btn, UI_COLOR_BORDER, 0);
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRESSED, LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, UI_COLOR_TEXT_DARK, 0);
    }
    lv_obj_set_style_text_font(btn, FONT_BUTTON, 0);
    lv_obj_set_style_pad_left(btn, 24, 0);
    lv_obj_set_style_pad_right(btn, 24, 0);
    lv_obj_set_style_min_height(btn, 44, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
}

// Create and style a msgbox with title and message
static lv_obj_t *ui_msgbox_create_styled(const std::string &title, const std::string &message)
{
    lv_obj_t *mbox = lv_msgbox_create(NULL);

    // Style the message box container
    lv_obj_set_style_bg_color(mbox, UI_COLOR_CARD, 0);
    lv_obj_set_style_border_color(mbox, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_border_side(mbox, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_clip_corner(mbox, false, 0);
    lv_obj_set_style_pad_all(mbox, 20, 0);
    lv_obj_set_style_pad_gap(mbox, 15, 0);

    // Add title with bold font
    lv_obj_t *title_label = lv_msgbox_add_title(mbox, title.c_str());
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, FONT_TITLE_BOLD, 0);
    // Reserve a little vertical breathing room to avoid glyph clipping.
    lv_obj_set_style_min_height(title_label, lv_font_get_line_height(FONT_TITLE_BOLD) + 8, 0);
    lv_obj_set_style_pad_top(title_label, 4, 0);
    lv_obj_set_style_pad_bottom(title_label, 4, 0);

    // Add message text
    lv_obj_t *text_label = lv_msgbox_add_text(mbox, message.c_str());
    lv_obj_set_style_text_color(text_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(text_label, FONT_BODY, 0);

    return mbox;
}

// Finalize msgbox styling (call after adding buttons)
static void ui_msgbox_finalize(lv_obj_t *mbox)
{
    // Get the parts and style them (after buttons have been created)
    lv_obj_t *header = lv_msgbox_get_header(mbox);
    lv_obj_t *content = lv_msgbox_get_content(mbox);
    lv_obj_t *footer = lv_msgbox_get_footer(mbox);

    // Remove grey background from header
    if (header) {
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_set_style_min_height(header, lv_font_get_line_height(FONT_TITLE_BOLD) + 8, 0);
        lv_obj_set_height(header, LV_SIZE_CONTENT);
    }

    // Make content and footer transparent
    if (content) {
        lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    }
    if (footer) {
        lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
    }

    lv_obj_center(mbox);
}

void ui_show_error(const std::string &title, const std::string &message)
{
    lv_obj_t *mbox = ui_msgbox_create_styled(title, message);

    // Add OK button
    lv_obj_t *btn = lv_msgbox_add_footer_button(mbox, "OK");
    style_msgbox_button(btn, true);
    lv_obj_add_event_cb(btn, msgbox_close_cb, LV_EVENT_CLICKED, mbox);

    ui_msgbox_finalize(mbox);
}

lv_obj_t *ui_show_confirm(const std::string &title, const std::string &message)
{
    lv_obj_t *mbox = ui_msgbox_create_styled(title, message);

    // Add Yes button (primary)
    lv_obj_t *yes_btn = lv_msgbox_add_footer_button(mbox, "Yes");
    style_msgbox_button(yes_btn, true);

    // Add No button (secondary)
    lv_obj_t *no_btn = lv_msgbox_add_footer_button(mbox, "No");
    style_msgbox_button(no_btn, false);

    ui_msgbox_finalize(mbox);
    return mbox;
}

lv_obj_t *ui_spinner_show(const std::string &text)
{
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE); // absorb clicks

    lv_obj_t *spinner = lv_spinner_create(overlay);
    lv_spinner_set_anim_params(spinner, 1000, 200);
    lv_obj_set_size(spinner, 60, 60);

    if (!text.empty()) {
        lv_obj_t *label = lv_label_create(overlay);
        lv_label_set_text(label, text.c_str());
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(label, FONT_INPUT, 0);
        lv_obj_set_style_pad_top(label, 20, 0);
    }

    return overlay;
}

void ui_spinner_hide(lv_obj_t *spinner_overlay)
{
    if (!spinner_overlay) {
        return;
    }

    // The overlay may already be gone if a screen transition cleaned layer-top
    // objects while an async operation was in flight.
    if (!lv_obj_is_valid(spinner_overlay)) {
        return;
    }

    // Async delete avoids tearing down an object while LVGL is still unwinding
    // nested input/event processing for that object.
    lv_obj_delete_async(spinner_overlay);
}

void ui_style_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, UI_COLOR_TEXT, 0);
}

lv_obj_t *ui_create_button(lv_obj_t *parent, const std::string &text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = ui_btn_create(parent);
    lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(btn, 12, 0);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text.c_str());
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, FONT_BUTTON, 0); // Use standard button font
    lv_obj_center(label);

    if (cb) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

    return btn;
}

void ui_set_test_id(lv_obj_t *obj, const std::string &test_id)
{
    if (!obj || test_id.empty()) {
        return;
    }
    lv_obj_set_name(obj, test_id.c_str());
}

lv_color_t ui_product_color(const std::string &product)
{
    switch (station_product_from_key(product)) {
    case StationProduct::BUS:
        return UI_COLOR_PRODUCT_BUS;
    case StationProduct::TRAM:
        return UI_COLOR_PRODUCT_TRAM;
    case StationProduct::SUBURBAN:
        return UI_COLOR_PRODUCT_SUBURBAN;
    case StationProduct::SUBWAY:
        return UI_COLOR_PRODUCT_SUBWAY;
    case StationProduct::FERRY:
        return UI_COLOR_PRODUCT_FERRY;
    case StationProduct::EXPRESS:
        return UI_COLOR_PRODUCT_EXPRESS;
    case StationProduct::REGIONAL:
        return UI_COLOR_PRODUCT_REGIONAL;
    case StationProduct::UNKNOWN:
    default:
        return UI_COLOR_GRAY_DEFAULT;
    }
}

void ui_clear_flag_recursive(lv_obj_t *obj, lv_obj_flag_t flag)
{
    if (!obj) {
        return;
    }
    lv_obj_clear_flag(obj, flag);
    uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; i++) {
        ui_clear_flag_recursive(lv_obj_get_child(obj, i), flag);
    }
}
