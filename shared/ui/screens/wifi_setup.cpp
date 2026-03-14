#include "wifi_setup.h"
#include "ui/callback_context.h"
#include "ui/common.h"
#include "fira_sans.h"
#include "esp_log.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <string>

static const char *TAG = "ui_wifi";

namespace {

std::string sanitize_ssid_for_test_id(const std::string &ssid)
{
    std::string out;
    out.reserve(ssid.size());
    for (unsigned char c : ssid) {
        if (std::isalnum(c) != 0) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        out = "unknown";
    }
    return out;
}

} // namespace

// ===== Callbacks =====

void WifiSetupScreen::network_item_cb(lv_event_t *e)
{
    auto *ctx = static_cast<NetworkClickContext *>(lv_event_get_user_data(e));

    ESP_LOGI(TAG, "Selected network: %s (open=%d)", ctx->ssid.c_str(), ctx->is_open);

    // For open networks, connect immediately without password prompt
    if (ctx->is_open) {
        // Clear the current network highlight since we're connecting to a different one
        ctx->clear_highlight();
        ctx->connect(ctx->ssid, ""); // Empty password for open networks
    } else {
        // For secured networks, show password popup
        ctx->show_password_popup(ctx->ssid);
    }
}

void WifiSetupScreen::connect_btn_cb(lv_event_t *e)
{
    auto *ctx = static_cast<PasswordPopupContext *>(lv_event_get_user_data(e));
    std::string ssid = ctx->ssid;
    auto clear_highlight = ctx->clear_highlight;
    auto connect = ctx->connect;
    std::string password = lv_textarea_get_text(ctx->password_ta);

    ctx->close_popup();

    // Clear the current network highlight since we're connecting to a different one
    clear_highlight();

    connect(ssid, password);
}

// ===== Helper methods =====

void WifiSetupScreen::clear_current_network_highlight()
{
    if (m_current_network_btn) {
        // Remove checkmark icon (first child of the button)
        uint32_t child_count = lv_obj_get_child_count(m_current_network_btn);
        if (child_count > 1) {
            // If there are 2+ children, the first is the checkmark icon
            lv_obj_t *icon = lv_obj_get_child(m_current_network_btn, 0);
            if (icon) {
                lv_obj_delete(icon);
            }
        }

        // Reset background color to normal
        lv_obj_set_style_bg_color(m_current_network_btn, UI_COLOR_CARD, 0);

        // Re-enable pressed state color
        lv_obj_set_style_bg_color(m_current_network_btn, UI_COLOR_PRESSED, LV_STATE_PRESSED);

        // Note: We don't re-add the click handler here because:
        // 1. The button will be regenerated on next scan anyway
        // 2. Adding it back would require storing NetworkInfo which gets complex
        // 3. Users typically don't click the same network again in the same session

        m_current_network_btn = nullptr;
    }
}

void WifiSetupScreen::close_password_popup()
{
    if (!m_password_popup && !m_password_backdrop) {
        return;
    }

    // Remove textarea from input group before deleting
    if (m_password_ta) {
        lv_group_t *group = lv_group_get_default();
        if (group) {
            lv_group_remove_obj(m_password_ta);
        }
    }

    if (m_password_backdrop) {
        lv_obj_delete(m_password_backdrop);
        m_password_backdrop = nullptr;
    } else if (m_password_popup) {
        lv_obj_delete(m_password_popup);
    }

    m_password_popup = nullptr;
    m_password_ta = nullptr;
}

void WifiSetupScreen::show_password_popup(const std::string &ssid)
{
    // Close existing popup if any
    close_password_popup();

    // Create modal backdrop to dim background and absorb clicks.
    m_password_backdrop = lv_obj_create(lv_layer_top());
    ui_set_test_id(m_password_backdrop, "wifi_setup.password_popup.backdrop");
    lv_obj_remove_style_all(m_password_backdrop);
    lv_obj_set_size(m_password_backdrop, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(m_password_backdrop, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_password_backdrop, LV_OPA_30, 0);
    lv_obj_add_flag(m_password_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(m_password_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    // Create modal popup - auto height based on content
    m_password_popup = lv_obj_create(m_password_backdrop);
    ui_set_test_id(m_password_popup, "wifi_setup.password_popup");

    lv_display_t *disp = lv_display_get_default();
    int32_t h_res = lv_display_get_horizontal_resolution(disp);
    int32_t v_res = lv_display_get_vertical_resolution(disp);
    int32_t short_edge = std::min(h_res, v_res);

    bool is_portrait = !ui_is_landscape();
    // Use a compact layout so keys are larger and spacing is tighter in both orientations.
    bool compact_small_board = short_edge <= 500;

    int width_pct = is_portrait ? (compact_small_board ? 96 : 92) : (compact_small_board ? 78 : 65);
    int popup_pad_x = compact_small_board ? 8 : 20;
    int popup_pad_y = compact_small_board ? 12 : 20;
    int popup_gap = 15;
    int keyboard_height = is_portrait ? 300 : (compact_small_board ? 200 : 300);

    // Keep keyboard from exceeding available popup height on very short displays.
    int max_keyboard_height = (v_res * 44) / 100;
    keyboard_height = std::min(keyboard_height, max_keyboard_height);
    if (keyboard_height < 160) {
        keyboard_height = 160;
    }

    lv_obj_set_size(m_password_popup, LV_PCT(width_pct), LV_SIZE_CONTENT);
    lv_obj_center(m_password_popup);
    lv_obj_set_style_bg_color(m_password_popup, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_password_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_password_popup, 2, 0);
    lv_obj_set_style_border_color(m_password_popup, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_side(m_password_popup, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_clip_corner(m_password_popup, false, 0);
    lv_obj_set_style_radius(m_password_popup, 12, 0);
    lv_obj_set_style_pad_left(m_password_popup, popup_pad_x, 0);
    lv_obj_set_style_pad_right(m_password_popup, popup_pad_x, 0);
    lv_obj_set_style_pad_top(m_password_popup, popup_pad_y, 0);
    lv_obj_set_style_pad_bottom(m_password_popup, popup_pad_y, 0);
    lv_obj_set_style_pad_gap(m_password_popup, popup_gap, 0);
    lv_obj_set_flex_flow(m_password_popup, LV_FLEX_FLOW_COLUMN);

    // SSID title row: one line with emphasized SSID
    lv_obj_t *ssid_row = lv_obj_create(m_password_popup);
    lv_obj_remove_style_all(ssid_row);
    lv_obj_set_width(ssid_row, LV_PCT(100));
    lv_obj_set_height(ssid_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ssid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ssid_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(ssid_row, 8, 0);

    lv_obj_t *ssid_prefix_label = lv_label_create(ssid_row);
    lv_label_set_text(ssid_prefix_label, "Connect to:");
    lv_obj_set_style_text_color(ssid_prefix_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(ssid_prefix_label, FONT_BODY, 0);

    lv_obj_t *ssid_value_label = lv_label_create(ssid_row);
    lv_label_set_text(ssid_value_label, ssid.c_str());
    lv_obj_set_style_text_color(ssid_value_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(ssid_value_label, FONT_BODY_BOLD, 0);

    // Password input
    m_password_ta = ui_textarea_create(m_password_popup);
    ui_set_test_id(m_password_ta, "wifi_setup.password_popup.password_input");
    lv_textarea_set_placeholder_text(m_password_ta, "Enter password...");
    lv_textarea_set_password_mode(m_password_ta, true);
    lv_textarea_set_one_line(m_password_ta, true);
    lv_obj_set_width(m_password_ta, LV_PCT(100));
    lv_obj_set_style_bg_color(m_password_ta, UI_COLOR_CARD, 0);
    lv_obj_set_style_text_font(m_password_ta, FONT_INPUT, 0);
    lv_obj_set_style_text_color(m_password_ta, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_border_color(m_password_ta, UI_COLOR_BORDER, 0);
    lv_obj_add_state(m_password_ta, LV_STATE_FOCUSED);

    // Add to default input group and focus for keyboard input
    lv_group_t *group = lv_group_get_default();
    if (group) {
        lv_group_add_obj(group, m_password_ta);
        lv_group_focus_obj(m_password_ta);
    }

    // Show password checkbox
    lv_obj_t *show_password_cb = lv_checkbox_create(m_password_popup);
    lv_checkbox_set_text(show_password_cb, "Show password");
    lv_obj_set_style_text_font(show_password_cb, FONT_BODY, 0);
    lv_obj_set_style_text_color(show_password_cb, UI_COLOR_TEXT_DARK, 0);
    // Style the checkbox indicator
    lv_obj_set_style_border_color(show_password_cb, UI_COLOR_BORDER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(show_password_cb, UI_COLOR_CARD, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(show_password_cb, UI_COLOR_PRIMARY,
                              static_cast<lv_style_selector_t>(static_cast<uint32_t>(LV_PART_INDICATOR) |
                                                               static_cast<uint32_t>(LV_STATE_CHECKED)));
    lv_obj_add_event_cb(
        show_password_cb,
        [](lv_event_t *e) {
            lv_obj_t *cb = (lv_obj_t *)lv_event_get_target(e);
            lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
            bool checked = lv_obj_get_state(cb) & LV_STATE_CHECKED;
            lv_textarea_set_password_mode(ta, !checked);
        },
        LV_EVENT_VALUE_CHANGED, m_password_ta);

    // Create keyboard for password input
    lv_obj_t *keyboard = lv_keyboard_create(m_password_popup);
    ui_set_test_id(keyboard, "wifi_setup.password_popup.keyboard");
    lv_obj_set_size(keyboard, LV_PCT(100), keyboard_height);
    if (compact_small_board) {
        // Tighter key gaps on the smaller board to increase touch target size.
        lv_obj_set_style_pad_column(keyboard, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_row(keyboard, 5, LV_PART_MAIN);
        lv_obj_set_style_pad_all(keyboard, 3, LV_PART_MAIN);
    }
    lv_obj_set_style_text_font(keyboard, FONT_BODY, LV_PART_ITEMS);
    lv_keyboard_set_textarea(keyboard, m_password_ta);

    // Button row
    lv_obj_t *btn_row = lv_obj_create(m_password_popup);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row, compact_small_board ? 10 : 15, 0);

    // Create context for popup callbacks and bind its lifetime to the popup.
    auto *popup_ctx = ui_make_callback_context<PasswordPopupContext>(
        m_password_popup, ssid, m_password_ta, [this]() { close_password_popup(); },
        [this]() { clear_current_network_highlight(); },
        [this](const std::string &ssid, const std::string &password) { m_on_connect_request(ssid, password); });
    if (!popup_ctx) {
        close_password_popup();
        return;
    }

    // Close button
    lv_obj_t *close_btn = ui_create_button(btn_row, "Close", ScreenBase::generic_callback_cb, &popup_ctx->close_popup);
    ui_set_test_id(close_btn, "wifi_setup.password_popup.close_button");
    lv_obj_set_width(close_btn, LV_PCT(45));

    // Connect button
    lv_obj_t *connect_btn = ui_create_button(btn_row, "Connect", connect_btn_cb, popup_ctx);
    ui_set_test_id(connect_btn, "wifi_setup.password_popup.connect_button");
    lv_obj_set_width(connect_btn, LV_PCT(45));

    // Initially disable connect button since password is empty
    lv_obj_add_state(connect_btn, LV_STATE_DISABLED);

    // Add event handler to enable/disable connect button based on password input
    lv_obj_add_event_cb(
        m_password_ta,
        [](lv_event_t *e) {
            lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
            lv_obj_t *btn = (lv_obj_t *)lv_event_get_user_data(e);
            const char *text = lv_textarea_get_text(ta);

            if (text && strlen(text) > 7) {
                lv_obj_remove_state(btn, LV_STATE_DISABLED);
            } else {
                lv_obj_add_state(btn, LV_STATE_DISABLED);
            }
        },
        LV_EVENT_VALUE_CHANGED, connect_btn);
}

// ===== Constructor and Destructor =====

WifiSetupScreen::WifiSetupScreen(
    std::function<void()> on_scan_request,
    std::function<void(const std::string &ssid, const std::string &password)> on_connect_request,
    std::function<void()> on_back, bool show_back_button)
    : m_on_scan_request(std::move(on_scan_request)), m_on_connect_request(std::move(on_connect_request)),
      m_on_back(std::move(on_back))
{
    m_screen = lv_obj_create(NULL);
    ui_set_test_id(m_screen, "wifi_setup.screen");
    ui_style_screen(m_screen);

    // ===== Network list view =====
    lv_obj_t *list_cont = lv_obj_create(m_screen);
    lv_obj_remove_style_all(list_cont);
    lv_obj_set_size(list_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_cont, UI_PADDING, 0);
    lv_obj_set_style_pad_gap(list_cont, 10, 0);

    // Header row with optional back button, title and scan button
    lv_obj_t *header_row = lv_obj_create(list_cont);
    lv_obj_remove_style_all(header_row);
    lv_obj_set_width(header_row, LV_PCT(100));
    lv_obj_set_height(header_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Back button (only shown when changing WiFi, not during initial setup)
    if (show_back_button) {
        lv_obj_t *back_btn = ui_create_button(header_row, "Back", ScreenBase::generic_callback_cb, &m_on_back);
        ui_set_test_id(back_btn, "wifi_setup.back_button");
    }

    // Title
    lv_obj_t *title = lv_label_create(header_row);
    lv_label_set_text(title, "Select WiFi Network");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_TITLE, 0);

    // Scan button
    lv_obj_t *scan_btn = ui_create_button(header_row, "Scan", ScreenBase::generic_callback_cb, &m_on_scan_request);
    ui_set_test_id(scan_btn, "wifi_setup.scan_button");

    // Network list
    m_network_list = lv_list_create(list_cont);
    ui_set_test_id(m_network_list, "wifi_setup.network_list");
    lv_obj_set_width(m_network_list, LV_PCT(100));
    lv_obj_set_flex_grow(m_network_list, 1);
    lv_obj_set_style_bg_color(m_network_list, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_network_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_network_list, 0, 0);
    lv_obj_set_style_radius(m_network_list, 8, 0);
    // Remove horizontal padding for full-width separators, keep small vertical padding for rounded corners
    lv_obj_set_style_pad_left(m_network_list, 0, 0);
    lv_obj_set_style_pad_right(m_network_list, 0, 0);
    lv_obj_set_style_pad_top(m_network_list, 4, 0);
    lv_obj_set_style_pad_bottom(m_network_list, 4, 0);
    lv_obj_set_style_clip_corner(m_network_list, true, 0); // Clip content to rounded corners
}

WifiSetupScreen::~WifiSetupScreen()
{
    close_password_popup();
    m_network_list = nullptr;
    if (m_screen) {
        lv_obj_delete(m_screen);
        m_screen = nullptr;
    }
}

void WifiSetupScreen::populate_networks(const UiWifiNetwork *networks, int count, const char *current_ssid)
{
    if (!m_network_list)
        return;

    lv_obj_clean(m_network_list);
    m_current_network_btn = nullptr; // Reset current network tracking

    if (count == 0) {
        lv_obj_t *label = lv_label_create(m_network_list);
        lv_label_set_text(label, "No networks found.");
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT_DIM, 0);
        return;
    }

    std::string current = current_ssid ? current_ssid : "";
    for (int i = 0; i < count; i++) {
        const auto &net = networks[i];
        if (net.ssid.empty())
            continue;

        // Check if this is the currently configured network
        bool is_current = !current.empty() && net.ssid == current;

        // Signal strength indicator with filled/unfilled circles
        const char *signal = (net.rssi > -50) ? "●●●●" : (net.rssi > -70) ? "●●●○" : "●●○○";

        // Button text with lock icon for secured networks
        std::string text = net.ssid + "  ";
        if (!net.is_open) {
            text += "\uF023  "; // Font Awesome lock icon
        }
        text += signal;

        // Create button with checkmark icon for current network
        lv_obj_t *btn = lv_list_add_button(m_network_list, is_current ? "✓" : nullptr, text.c_str());
        lv_obj_set_style_bg_color(btn, is_current ? UI_COLOR_HIGHLIGHT : UI_COLOR_CARD, 0);
        std::string network_id = "wifi_setup.network." + sanitize_ssid_for_test_id(net.ssid);
        ui_set_test_id(btn, network_id);

        // Allocate network click context and bind its lifetime to the button.
        auto *ctx = ui_manage_callback_context(
            btn, new NetworkClickContext{net.ssid, net.is_open, [this]() { clear_current_network_highlight(); },
                                         [this](const std::string &ssid, const std::string &password) {
                                             m_on_connect_request(ssid, password);
                                         },
                                         [this](const std::string &ssid) { show_password_popup(ssid); }});
        if (!ctx) {
            lv_obj_delete(btn);
            continue;
        }

        // Track the current network button
        if (is_current) {
            m_current_network_btn = btn;
        }

        // Add pressed state color and click handler to all networks
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRESSED, LV_STATE_PRESSED);
        lv_obj_add_event_cb(btn, network_item_cb, LV_EVENT_CLICKED, ctx);

        lv_obj_set_style_text_color(btn, UI_COLOR_TEXT_DARK, 0);
        lv_obj_set_style_text_font(btn, FONT_BODY, 0);
    }
}

void WifiSetupScreen::show_connect_error(const std::string &message)
{
    ui_show_error("Connection Failed", message);
}
