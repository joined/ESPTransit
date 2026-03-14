#include "settings.h"
#include "app_constants.h"
#include "ui/callback_context.h"
#include "ui/common.h"
#include "ui/components/card.h"
#include "ui/components/station_badges.h"
#include "app_platform.h"
#include "esp_log.h"
#include <format>
#include <string>

static const char *TAG = "ui_settings";

namespace {

std::string build_rotation_options()
{
    if (ui_is_native_landscape()) {
        return "0° (Landscape, Fast HW)\n90° (Portrait, Slower SW)\n180° (Landscape Inverted, Fast HW)\n270° (Portrait "
               "Inverted, Slower SW)";
    }

    return "0° (Portrait, Fast HW)\n90° (Landscape, Slower SW)\n180° (Portrait Inverted, Fast HW)\n270° (Landscape "
           "Inverted, Slower SW)";
}

} // namespace

// ===== Callbacks =====

void SettingsScreen::rotation_confirm_yes_cb(lv_event_t *e)
{
    lv_obj_t *mbox = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    auto *ctx = static_cast<SettingsScreen::RotationConfirmContext *>(lv_obj_get_user_data(mbox));
    if (!ctx) {
        lv_msgbox_close_async(mbox);
        return;
    }

    // User confirmed - apply the rotation change
    *ctx->rotation_ctx->current_rotation = ctx->new_rotation;
    ESP_LOGI(TAG, "Rotation confirmed: %d", *ctx->rotation_ctx->current_rotation);
    ctx->rotation_ctx->on_rotation_changed(*ctx->rotation_ctx->current_rotation);

    lv_msgbox_close_async(mbox);
}

void SettingsScreen::rotation_confirm_no_cb(lv_event_t *e)
{
    lv_obj_t *mbox = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    auto *ctx = static_cast<SettingsScreen::RotationConfirmContext *>(lv_obj_get_user_data(mbox));
    if (!ctx) {
        lv_msgbox_close_async(mbox);
        return;
    }

    // User cancelled - revert dropdown to previous value (current_rotation was never changed)
    ESP_LOGI(TAG, "Rotation cancelled");
    if (ctx->dropdown) {
        lv_dropdown_set_selected(ctx->dropdown, ctx->old_rotation);
    }

    lv_msgbox_close_async(mbox);
}

void SettingsScreen::rotation_changed_cb(lv_event_t *e)
{
    auto *ctx = static_cast<SettingsScreen::RotationContext *>(lv_event_get_user_data(e));
    lv_obj_t *dropdown = (lv_obj_t *)lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    uint8_t new_rotation = static_cast<uint8_t>(selected);

    // Only notify if the value actually changed
    if (new_rotation == *ctx->current_rotation) {
        return;
    }

    ESP_LOGI(TAG, "Rotation change requested from %d to %d", *ctx->current_rotation, new_rotation);

    // Determine if either old or new rotation requires SW rotation (90° or 270°)
    bool old_needs_sw = (*ctx->current_rotation == 1 || *ctx->current_rotation == 3);
    bool new_needs_sw = (new_rotation == 1 || new_rotation == 3);

    const char *message;
    if (!old_needs_sw && new_needs_sw) {
        // Switching TO 90°/270° - will be slower
        message = "Changing to 90° or 270° rotation requires a device reboot "
                  "and will result in slower performance due to software rotation.\n\n"
                  "Continue?";
    } else if (old_needs_sw && !new_needs_sw) {
        // Switching FROM 90°/270° to 0°/180° - will be faster
        message = "Changing to 0° or 180° rotation requires a device reboot "
                  "and will enable faster hardware-accelerated rotation.\n\n"
                  "Continue?";
    } else {
        // Both use same rotation mode - just notify directly
        *ctx->current_rotation = new_rotation;
        ctx->on_rotation_changed(*ctx->current_rotation);
        return;
    }

    // Show confirmation dialog
    lv_obj_t *mbox = ui_show_confirm("Rotation Change", message);
    ui_set_test_id(mbox, "settings.rotation_confirm_popup");

    // Allocate context for confirmation callbacks
    auto *confirm_ctx = ui_make_callback_context<SettingsScreen::RotationConfirmContext>(mbox, *ctx->current_rotation,
                                                                                         new_rotation, dropdown, ctx);
    if (!confirm_ctx) {
        lv_msgbox_close_async(mbox);
        return;
    }
    lv_obj_set_user_data(mbox, confirm_ctx);

    // Add event callbacks to the footer buttons
    lv_obj_t *footer = lv_msgbox_get_footer(mbox);
    if (footer && lv_obj_get_child_count(footer) >= 2) {
        lv_obj_t *yes_btn = lv_obj_get_child(footer, 0); // First button is "Yes"
        lv_obj_t *no_btn = lv_obj_get_child(footer, 1);  // Second button is "No"
        ui_set_test_id(yes_btn, "settings.rotation_confirm_popup.yes_button");
        ui_set_test_id(no_btn, "settings.rotation_confirm_popup.no_button");
        lv_obj_add_event_cb(yes_btn, rotation_confirm_yes_cb, LV_EVENT_CLICKED, mbox);
        lv_obj_add_event_cb(no_btn, rotation_confirm_no_cb, LV_EVENT_CLICKED, mbox);
    }
}

void SettingsScreen::brightness_changed_cb(lv_event_t *e)
{
    auto *ctx = static_cast<SettingsScreen::BrightnessContext *>(lv_event_get_user_data(e));
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    // Enforce minimum brightness of 1%
    if (value < 1) {
        value = 1;
        lv_slider_set_value(slider, value, LV_ANIM_ON);
    }

    *ctx->current_brightness = static_cast<uint8_t>(value);
    ESP_LOGI(TAG, "Brightness changed to %d%%", *ctx->current_brightness);

    ctx->on_brightness_changed(*ctx->current_brightness);
}

void SettingsScreen::split_mode_changed_cb(lv_event_t *e)
{
    auto *ctx = static_cast<SettingsScreen::SplitModeContext *>(lv_event_get_user_data(e));
    if (!ctx || !ctx->current_split_mode) {
        return;
    }

    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (enabled == *ctx->current_split_mode) {
        return;
    }

    *ctx->current_split_mode = enabled;
    ESP_LOGI(TAG, "Split mode changed: %d", enabled ? 1 : 0);

    lv_obj_t *label = (lv_obj_t *)lv_obj_get_user_data(sw);
    if (label) {
        lv_label_set_text(label, enabled ? "Enabled" : "Disabled");
    }

    ctx->on_split_mode_changed(enabled);
}

void SettingsScreen::version_tap_cb(lv_event_t *e)
{
    auto *self = static_cast<SettingsScreen *>(lv_event_get_user_data(e));
    uint32_t now = lv_tick_get();

    if (now - self->m_last_tap_time > 1000) {
        self->m_version_tap_count = 0;
    }
    self->m_last_tap_time = now;
    self->m_version_tap_count++;

    if (self->m_version_tap_count >= 3) {
        self->m_version_tap_count = 0;
        if (self->m_on_debug) {
            self->m_on_debug();
        }
    }
}

void SettingsScreen::update()
{
    bool is_connected = m_is_wifi_connected();
    lv_label_set_text(m_wifi_connected_label, is_connected ? "Yes" : "No");
}

// ===== Helper methods =====

lv_obj_t *SettingsScreen::build_card(lv_obj_t *parent)
{
    return ui_create_card(parent, 16, 8, 8);
}

void SettingsScreen::build_departure_stations_section(lv_obj_t *parent, lv_event_cb_t btn_cb, void *callback_data)
{
    lv_obj_t *card = build_card(parent);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, "Departure Stations");
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, ui_font_settings_section_title(), 0);

    if (m_stations.empty()) {
        lv_obj_t *empty_label = lv_label_create(card);
        lv_label_set_text(empty_label, "No stations configured");
        lv_obj_set_style_text_color(empty_label, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(empty_label, FONT_MEDIUM, 0);
    } else {
        lv_obj_t *configured_stations_container = lv_obj_create(card);
        lv_obj_remove_style_all(configured_stations_container);
        lv_obj_set_width(configured_stations_container, LV_PCT(100));
        lv_obj_set_height(configured_stations_container, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(configured_stations_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_ver(configured_stations_container, 12, 0);
        lv_obj_set_style_pad_gap(configured_stations_container, 2, 0);

        for (const auto &station : m_stations) {
            lv_obj_t *item = lv_obj_create(configured_stations_container);
            lv_obj_remove_style_all(item);
            lv_obj_set_width(item, LV_PCT(100));
            lv_obj_set_height(item, LV_SIZE_CONTENT);
            lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_all(item, 10, 0);
            lv_obj_set_style_pad_gap(item, 10, 0);
            lv_obj_set_style_border_width(item, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(item, UI_COLOR_BORDER, LV_PART_MAIN);
            lv_obj_set_style_bg_color(item, UI_COLOR_HIGHLIGHT, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(item, 6, 0);
            lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t *name_label = lv_label_create(item);
            lv_label_set_text(name_label, station.name.c_str());
            lv_label_set_long_mode(name_label, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(name_label, LV_PCT(40));
            lv_obj_set_style_text_color(name_label, UI_COLOR_TEXT_DARK, 0);
            lv_obj_set_style_text_font(name_label, FONT_BODY, 0);
            lv_obj_clear_flag(name_label, LV_OBJ_FLAG_CLICKABLE);

            ui_create_station_badges(item, station.lines, LV_PCT(60), true, "Lines unavailable");
        }
    }

    lv_obj_t *manage_stations_btn = ui_create_button(card, "Manage Stations", btn_cb, callback_data);
    ui_set_test_id(manage_stations_btn, "settings.manage_stations_button");
}

void SettingsScreen::build_wifi_section(lv_obj_t *parent, const std::string &ssid, bool wifi_connected,
                                        lv_event_cb_t btn_cb, void *callback_data)
{
    lv_obj_t *card = build_card(parent);

    // Title
    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, "WiFi");
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, ui_font_settings_section_title(), 0);

    // Network name (static)
    lv_obj_t *network_label = lv_label_create(card);
    if (!ssid.empty()) {
        lv_label_set_text(network_label, std::format("Network: {}", ssid).c_str());
    } else {
        lv_label_set_text(network_label, "Network: None");
    }
    lv_obj_set_style_text_color(network_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(network_label, FONT_MEDIUM, 0);

    // Connection status container (label that will be updated)
    lv_obj_t *connected_container = lv_obj_create(card);
    lv_obj_remove_style_all(connected_container);
    lv_obj_set_width(connected_container, LV_PCT(100));
    lv_obj_set_height(connected_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(connected_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(connected_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(connected_container, 4, 0);

    lv_obj_t *connected_text = lv_label_create(connected_container);
    lv_label_set_text(connected_text, "Connected:");
    lv_obj_set_style_text_color(connected_text, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(connected_text, FONT_MEDIUM, 0);

    m_wifi_connected_label = lv_label_create(connected_container);
    lv_label_set_text(m_wifi_connected_label, wifi_connected ? "Yes" : "No");
    lv_obj_set_style_text_color(m_wifi_connected_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(m_wifi_connected_label, FONT_MEDIUM, 0);

    lv_obj_t *change_wifi_btn = ui_create_button(card, "Change WiFi Network", btn_cb, callback_data);
    ui_set_test_id(change_wifi_btn, "settings.change_wifi_button");
}

void SettingsScreen::build_dropdown_section(lv_obj_t *parent, const std::string &title, const std::string &options,
                                            uint16_t selected, lv_event_cb_t change_cb, void *user_data,
                                            const std::string &test_id)
{
    lv_obj_t *card = build_card(parent);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title.c_str());
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, ui_font_settings_section_title(), 0);

    lv_obj_t *dropdown = lv_dropdown_create(card);
    lv_dropdown_set_options(dropdown, options.c_str());
    lv_dropdown_set_selected(dropdown, selected);
    lv_obj_set_width(dropdown, LV_PCT(100));
    if (!test_id.empty()) {
        ui_set_test_id(dropdown, test_id);
    }

    // Style the dropdown button (selected value)
    lv_obj_set_style_text_font(dropdown, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(dropdown, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_bg_color(dropdown, UI_COLOR_CARD, 0);
    lv_obj_set_style_border_color(dropdown, UI_COLOR_BORDER, 0);

    // Style the dropdown list
    lv_obj_t *list = lv_dropdown_get_list(dropdown);
    lv_obj_set_style_text_font(list, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(list, UI_COLOR_TEXT_DARK, 0);
    if (!test_id.empty()) {
        ui_set_test_id(list, test_id + ".list");
    }

    lv_obj_add_event_cb(dropdown, change_cb, LV_EVENT_VALUE_CHANGED, user_data);
}

void SettingsScreen::build_slider_section(lv_obj_t *parent, const std::string &title, int32_t min_value,
                                          int32_t max_value, int32_t current_value, const std::string &unit,
                                          lv_event_cb_t change_cb, void *user_data)
{
    lv_obj_t *card = build_card(parent);

    // Title with current value
    lv_obj_t *title_container = lv_obj_create(card);
    lv_obj_remove_style_all(title_container);
    lv_obj_set_width(title_container, LV_PCT(100));
    lv_obj_set_height(title_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title_label = lv_label_create(title_container);
    lv_label_set_text(title_label, title.c_str());
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, ui_font_settings_section_title(), 0);

    lv_obj_t *value_label = lv_label_create(title_container);
    lv_label_set_text(value_label, std::format("{}{}", current_value, unit).c_str());
    lv_obj_set_style_text_color(value_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(value_label, FONT_MEDIUM, 0);

    // Slider
    lv_obj_t *slider = lv_slider_create(card);
    lv_slider_set_range(slider, min_value, max_value);
    lv_slider_set_value(slider, current_value, LV_ANIM_OFF);
    lv_obj_set_width(slider, LV_PCT(100));

    // Style the slider
    lv_obj_set_style_bg_color(slider, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, UI_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, UI_COLOR_PRIMARY, LV_PART_KNOB);

    // Store value label as user data so we can update it
    lv_obj_set_user_data(slider, value_label);

    // Add event callback that also updates the value label
    lv_obj_add_event_cb(
        slider,
        [](lv_event_t *e) {
            lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
            lv_obj_t *value_label = (lv_obj_t *)lv_obj_get_user_data(slider);
            int32_t value = lv_slider_get_value(slider);
            lv_label_set_text(value_label, std::format("{}%", value).c_str());
        },
        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_add_event_cb(slider, change_cb, LV_EVENT_VALUE_CHANGED, user_data);
}

void SettingsScreen::build_switch_section(lv_obj_t *parent, const std::string &title, const std::string &status,
                                          bool checked, lv_event_cb_t change_cb, void *user_data)
{
    lv_obj_t *card = build_card(parent);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title.c_str());
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, ui_font_settings_section_title(), 0);

    if (!status.empty()) {
        lv_obj_t *status_label = lv_label_create(card);
        lv_label_set_text(status_label, status.c_str());
        lv_obj_set_style_text_color(status_label, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(status_label, FONT_MEDIUM, 0);
    }

    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *mode_label = lv_label_create(row);
    lv_label_set_text(mode_label, checked ? "Enabled" : "Disabled");
    lv_obj_set_style_text_color(mode_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(mode_label, FONT_MEDIUM, 0);

    lv_obj_t *sw = lv_switch_create(row);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_set_user_data(sw, mode_label);
    lv_obj_add_event_cb(sw, change_cb, LV_EVENT_VALUE_CHANGED, user_data);
}

// ===== Constructor and Destructor =====

void SettingsScreen::font_size_changed_cb(lv_event_t *e)
{
    auto *ctx = static_cast<SettingsScreen::FontSizeContext *>(lv_event_get_user_data(e));
    lv_obj_t *dropdown = (lv_obj_t *)lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    uint8_t new_font_size = static_cast<uint8_t>(selected);

    if (new_font_size == *ctx->current_font_size) {
        return;
    }

    *ctx->current_font_size = new_font_size;
    ESP_LOGI(TAG, "Font size changed to %d", new_font_size);
    ctx->on_font_size_changed(new_font_size);
}

SettingsScreen::SettingsScreen(std::function<void()> on_back, std::function<void()> on_change_wifi,
                               std::function<void()> on_change_station,
                               std::function<void(uint8_t)> on_rotation_changed,
                               std::function<void(uint8_t)> on_brightness_changed,
                               std::function<void(bool)> on_split_mode_changed,
                               std::function<void(uint8_t)> on_font_size_changed,
                               std::function<bool()> is_wifi_connected, std::function<void()> on_debug,
                               const std::string &ssid, const std::vector<StationResult> &stations, uint8_t rotation,
                               uint8_t brightness, bool split_mode_enabled, uint8_t font_size)
    : m_on_back(std::move(on_back)), m_on_change_wifi(std::move(on_change_wifi)),
      m_on_change_station(std::move(on_change_station)), m_on_rotation_changed(std::move(on_rotation_changed)),
      m_on_brightness_changed(std::move(on_brightness_changed)),
      m_on_split_mode_changed(std::move(on_split_mode_changed)),
      m_on_font_size_changed(std::move(on_font_size_changed)), m_is_wifi_connected(std::move(is_wifi_connected)),
      m_current_rotation(rotation), m_current_brightness(brightness > 0 ? brightness : APP_CONFIG_DEFAULT_BRIGHTNESS),
      m_current_split_mode(split_mode_enabled), m_current_font_size(font_size),
      m_rotation_context{&m_current_rotation, m_on_rotation_changed},
      m_brightness_context{&m_current_brightness, m_on_brightness_changed},
      m_split_mode_context{&m_current_split_mode, m_on_split_mode_changed},
      m_font_size_context{&m_current_font_size, m_on_font_size_changed}, m_wifi_connected(m_is_wifi_connected()),
      m_on_debug(std::move(on_debug)), m_ssid(ssid), m_stations(stations),
      m_split_mode_available(m_stations.size() >= SPLIT_MODE_MIN_STATIONS &&
                             m_stations.size() <= SPLIT_MODE_MAX_STATIONS)
{
    m_screen = lv_obj_create(NULL);
    ui_set_test_id(m_screen, "settings.screen");
    ui_style_screen(m_screen);
    lv_obj_set_flex_flow(m_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_screen, UI_PADDING, 0);
    lv_obj_set_style_pad_gap(m_screen, 16, 0);

    // Header
    lv_obj_t *header = lv_obj_create(m_screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(header, 12, 0);

    lv_obj_t *back_btn = ui_create_button(header, LV_SYMBOL_LEFT " Back", ScreenBase::generic_callback_cb, &m_on_back);
    ui_set_test_id(back_btn, "settings.back_button");

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, ui_font_settings_title(), 0);

    // WiFi section - separate network name and connection status labels
    build_wifi_section(m_screen, m_ssid, m_wifi_connected, ScreenBase::generic_callback_cb, &m_on_change_wifi);

    // Station section
    build_departure_stations_section(m_screen, ScreenBase::generic_callback_cb, &m_on_change_station);

    if (m_split_mode_available) {
        build_switch_section(m_screen, "Split Mode", std::format("Show {} stations at once", m_stations.size()),
                             m_current_split_mode, split_mode_changed_cb, &m_split_mode_context);
    }

    // Brightness section (pass brightness context - callback only needs current brightness and callback)
    build_slider_section(m_screen, "Screen Brightness", 1, 100, m_current_brightness, "%", brightness_changed_cb,
                         &m_brightness_context);

    // Font Size section
    build_dropdown_section(m_screen, "Font Size", "Small\nMedium\nLarge", m_current_font_size, font_size_changed_cb,
                           &m_font_size_context, "settings.font_size_dropdown");

    // Screen Rotation section (pass rotation context - callback only needs current rotation and callback)
    build_dropdown_section(m_screen, "Screen Rotation", build_rotation_options(), m_current_rotation,
                           rotation_changed_cb, &m_rotation_context, "settings.rotation_dropdown");

    // Spacer to push version to bottom
    lv_obj_t *spacer = lv_obj_create(m_screen);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_width(spacer, LV_PCT(100));
    lv_obj_set_flex_grow(spacer, 1);

    // Version hitbox at bottom-left (triple-tap to open debug screen)
    lv_obj_t *version_hitbox = lv_obj_create(m_screen);
    lv_obj_remove_style_all(version_hitbox);
    lv_obj_set_width(version_hitbox, LV_PCT(100));
    lv_obj_set_height(version_hitbox, 36);
    ui_set_test_id(version_hitbox, "settings.version_hitbox");
    lv_obj_add_flag(version_hitbox, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(version_hitbox, version_tap_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *version_label = lv_label_create(version_hitbox);
    lv_label_set_text(version_label, AppPlatform::get_version_string());
    lv_obj_set_style_text_color(version_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(version_label, FONT_SMALL, 0);
    lv_obj_align(version_label, LV_ALIGN_LEFT_MID, 0, 0);

    // Disable scroll-on-focus for all children. This is a touchscreen device
    // with no keyboard/encoder navigation, so auto-scrolling on focus is not
    // needed and causes non-deterministic scroll positions on smaller displays.
    ui_clear_flag_recursive(m_screen, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
}

SettingsScreen::~SettingsScreen()
{
    m_wifi_connected_label = nullptr;

    if (m_screen) {
        lv_obj_delete(m_screen);
        m_screen = nullptr;
    }
}
