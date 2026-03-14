#pragma once

#include "lvgl.h"
#include <cstddef>
#include <string>

// Common UI metrics
#define UI_PADDING 20
#define UI_HEADER_HEIGHT 60
#define UI_KB_HEIGHT 400

// Colors
#define UI_COLOR_BG lv_color_hex(0x000000)                   // #000000 Black
#define UI_COLOR_CARD lv_color_hex(0xffffff)                 // #ffffff White
#define UI_COLOR_PRIMARY lv_color_hex(0x333333)              // #333333 Dark gray
#define UI_COLOR_ACCENT lv_color_hex(0x000000)               // #000000 Black
#define UI_COLOR_TEXT lv_color_hex(0xffffff)                 // #ffffff White
#define UI_COLOR_TEXT_DARK lv_color_hex(0x000000)            // #000000 Black
#define UI_COLOR_TEXT_DIM lv_color_hex(0x888888)             // #888888 Gray
#define UI_COLOR_BORDER lv_color_hex(0xdddddd)               // #dddddd Light gray
#define UI_COLOR_PRESSED lv_color_hex(0xcccccc)              // #cccccc Light gray
#define UI_COLOR_HIGHLIGHT lv_color_hex(0xf5f5f5)            // #f5f5f5 Very light gray
#define UI_COLOR_GRAY_DEFAULT lv_color_hex(0x666666)         // #666666 Medium gray
#define UI_COLOR_RED lv_color_hex(0xf44336)                  // #f44336 Red
#define UI_COLOR_BUTTON_DISABLED_BG lv_color_hex(0x4a4a4a)   // #4a4a4a Dark gray
#define UI_COLOR_BUTTON_DISABLED_TEXT lv_color_hex(0xd8d8d8) // #d8d8d8 Light gray

// Tab switcher colors
#define UI_COLOR_TAB_BG lv_color_hex(0x202020)            // #202020 Very dark gray
#define UI_COLOR_TAB_BG_PRESSED lv_color_hex(0x303030)    // #303030 Dark gray
#define UI_COLOR_TAB_BG_ACTIVE lv_color_hex(0x3a3a3a)     // #3a3a3a Active dark gray
#define UI_COLOR_TAB_BORDER lv_color_hex(0x4a4a4a)        // #4a4a4a Border dark gray
#define UI_COLOR_TAB_BORDER_ACTIVE lv_color_hex(0x6a6a6a) // #6a6a6a Active border gray
#define UI_COLOR_TAB_TEXT_INACTIVE lv_color_hex(0xc8c8c8) // #c8c8c8 Inactive text gray

// LVGL recolor tag payloads (without surrounding '#')
#define UI_RECOLOR_ERROR_HEX "FF0000"

// Transit product colors
#define UI_COLOR_PRODUCT_BUS lv_color_hex(0x993399)      // #993399 Purple
#define UI_COLOR_PRODUCT_TRAM lv_color_hex(0xcc0000)     // #cc0000 Red
#define UI_COLOR_PRODUCT_SUBURBAN lv_color_hex(0x37874a) // #37874a Green
#define UI_COLOR_PRODUCT_SUBWAY lv_color_hex(0x224f86)   // #224f86 Blue
#define UI_COLOR_PRODUCT_FERRY lv_color_hex(0x224f86)    // #224f86 Blue
#define UI_COLOR_PRODUCT_EXPRESS lv_color_hex(0xe21900)  // #e21900 Red
#define UI_COLOR_PRODUCT_REGIONAL lv_color_hex(0xe21900) // #e21900 Red

// Font size presets: Small (10.1"), Medium (7"), Large (4.3")
enum class FontSize : uint8_t { SMALL = 0, MEDIUM = 1, LARGE = 2 };

void ui_set_font_size(FontSize size);
FontSize ui_get_font_size();

// Preset-aware font getters — return the current preset's font pointer
const lv_font_t *ui_font_title();
const lv_font_t *ui_font_title_bold();
const lv_font_t *ui_font_body();
const lv_font_t *ui_font_body_bold();
const lv_font_t *ui_font_button();
const lv_font_t *ui_font_input();
const lv_font_t *ui_font_medium();
const lv_font_t *ui_font_small();
const lv_font_t *ui_font_logo();
const lv_font_t *ui_font_settings_title();
const lv_font_t *ui_font_settings_section_title();
const lv_font_t *ui_font_departures_split_title();

// Compatibility macros — existing call sites keep working
#define FONT_TITLE ui_font_title()
#define FONT_TITLE_BOLD ui_font_title_bold()
#define FONT_BUTTON ui_font_button()
#define FONT_INPUT ui_font_input()
#define FONT_BODY ui_font_body()
#define FONT_BODY_BOLD ui_font_body_bold()
#define FONT_MEDIUM ui_font_medium()
#define FONT_SMALL ui_font_small()
#define FONT_LOGO ui_font_logo()

// Show a modal error message box. auto-closes after timeout_ms (0 = manual close only)
void ui_show_error(const std::string &title, const std::string &message);

// Show a styled confirmation dialog with Yes/No buttons
// Returns the msgbox object. Use lv_obj_add_event_cb with LV_EVENT_VALUE_CHANGED to handle response.
lv_obj_t *ui_show_confirm(const std::string &title, const std::string &message);

// Create/show a fullscreen spinner overlay. Returns the overlay object.
lv_obj_t *ui_spinner_show(const std::string &text = {});

// Hide and delete the spinner overlay
void ui_spinner_hide(lv_obj_t *spinner_overlay);

// Create a button without the default glow/shadow effect
lv_obj_t *ui_btn_create(lv_obj_t *parent);

// Style a screen with dark background
void ui_style_screen(lv_obj_t *scr);

// Create a styled button
lv_obj_t *ui_create_button(lv_obj_t *parent, const std::string &text, lv_event_cb_t cb, void *user_data);

// Attach a stable test selector to a widget (no-op when object names are disabled).
void ui_set_test_id(lv_obj_t *obj, const std::string &test_id);

// Get BVG product color (bus, subway, tram, suburban, ferry, express, regional)
lv_color_t ui_product_color(const std::string &product);

// Measure rendered text width in pixels using glyph metrics and kerning.
int32_t ui_text_width_px(const lv_font_t *font, const std::string &text);

// Return how many repeats of sample_text fit into max_width_px.
size_t ui_max_repeated_text_count_for_width_px(const lv_font_t *font, const std::string &sample_text,
                                               int32_t max_width_px);

// Create a textarea and auto-apply test stabilization in UI test mode.
lv_obj_t *ui_textarea_create(lv_obj_t *parent);

// Make textarea caret rendering deterministic in UI tests (avoids blinking cursor screenshot flake).
void ui_stabilize_textarea_for_tests(lv_obj_t *textarea);

// Recursively clear an LVGL flag on an object and all its children.
void ui_clear_flag_recursive(lv_obj_t *obj, lv_obj_flag_t flag);

// Return true when the active display is wider than tall.
bool ui_is_landscape();

// Return true when the panel's native 0° orientation is landscape.
bool ui_is_native_landscape();
