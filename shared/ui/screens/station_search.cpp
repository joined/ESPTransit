#include "station_search.h"
#include "app_constants.h"
#include "ui/callback_context.h"
#include "ui/common.h"
#include "ui/components/station_badges.h"
#include "fira_sans.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <unordered_set>

static const char *TAG = "ui_station";

// ===== Callbacks =====

void StationSearchScreen::search_timer_cb(lv_timer_t *timer)
{
    auto *screen = static_cast<StationSearchScreen *>(lv_timer_get_user_data(timer));
    // Timer is auto-deleted after firing (repeat_count=1), so clear our pointer
    screen->m_search_timer = nullptr;
    const char *text = lv_textarea_get_text(screen->m_query_ta);
    if (text && strlen(text) >= STATION_SEARCH_MIN_QUERY_CHARS) {
        screen->m_on_search_request(text);
    }
}

void StationSearchScreen::result_item_cb(lv_event_t *e)
{
    static lv_point_t press_point;
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_active();
        if (indev)
            lv_indev_get_point(indev, &press_point);
        return;
    }

    if (code != LV_EVENT_RELEASED)
        return;

    // Ignore press-and-drag gestures by checking distance from press point
    lv_indev_t *indev = lv_indev_active();
    if (indev) {
        lv_point_t release_point;
        lv_indev_get_point(indev, &release_point);
        int32_t dx = release_point.x - press_point.x;
        int32_t dy = release_point.y - press_point.y;
        if (dx * dx + dy * dy > 100) // 10px threshold
            return;
    }

    auto *ctx = static_cast<ResultItemContext *>(lv_event_get_user_data(e));
    StationSearchScreen *const screen = ctx ? ctx->screen : nullptr;
    if (!screen) {
        return;
    }

    // set_station_selected() can delete the clicked row (and its callback context),
    // so copy everything needed before mutating selection state.
    const std::string id = ctx->id;
    const std::string name = ctx->name;
    const bool in_two_view_search_mode = screen->m_two_view && screen->m_two_view->in_search_mode;
    if (in_two_view_search_mode) {
        screen->set_station_selected(id, name, true);
        screen->exit_search_mode();
        ESP_LOGI(TAG, "Selected station: %s (id=%s)", name.c_str(), id.c_str());
        return;
    }

    bool selected = !screen->is_station_selected(id);
    screen->set_station_selected(id, name, selected);
    ESP_LOGI(TAG, "%s station: %s (id=%s)", selected ? "Selected" : "Deselected", name.c_str(), id.c_str());
}

void StationSearchScreen::ta_event_cb(lv_event_t *e)
{
    auto *screen = static_cast<StationSearchScreen *>(lv_event_get_user_data(e));
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED) {
        screen->set_keyboard_visible(true);
    } else if (code == LV_EVENT_DEFOCUSED) {
        screen->set_keyboard_visible(false);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        // User is typing - reset and start debounce timer
        const char *text = lv_textarea_get_text(screen->m_query_ta);
        size_t len = strlen(text);

        if (len < STATION_SEARCH_MIN_QUERY_CHARS) {
            // Input cleared or too short - clear results list and cancel any in-flight search
            screen->hide_loading();
            screen->clear_results();
            screen->m_on_search_cancel();
        } else {
            if (screen->m_search_timer) {
                lv_timer_reset(screen->m_search_timer);
            } else {
                screen->m_search_timer = lv_timer_create(search_timer_cb, STATION_SEARCH_DEBOUNCE_MS, screen);
                lv_timer_set_repeat_count(screen->m_search_timer, 1);
            }
        }
    }
}

void StationSearchScreen::done_button_cb(lv_event_t *e)
{
    auto *screen = static_cast<StationSearchScreen *>(lv_event_get_user_data(e));
    auto selected = screen->get_selected_stations();
    if (selected.empty()) {
        ui_show_error("No Stations Selected", "Select at least one station before continuing.");
        return;
    }
    screen->m_on_selection_done(selected);
}

// ===== Helper methods =====

void StationSearchScreen::set_keyboard_visible(bool show)
{
    if (!m_keyboard)
        return;
    if (show && m_two_view && !m_two_view->in_search_mode)
        return;

    if (show) {
        lv_keyboard_set_textarea(m_keyboard, m_query_ta);
        lv_obj_remove_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
        if (m_query_ta)
            lv_obj_add_state(m_query_ta, LV_STATE_FOCUSED);
        if (m_content)
            lv_obj_set_style_pad_bottom(m_content, m_kb_height, 0);
    } else {
        if (m_content)
            lv_obj_set_style_pad_bottom(m_content, 0, 0);
        if (m_query_ta)
            lv_obj_remove_state(m_query_ta, LV_STATE_FOCUSED);
        lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void StationSearchScreen::enter_search_mode()
{
    if (!m_two_view || m_two_view->in_search_mode)
        return;

    m_two_view->in_search_mode = true;

    if (m_two_view->configured_section)
        lv_obj_add_flag(m_two_view->configured_section, LV_OBJ_FLAG_HIDDEN);
    if (m_two_view->search_section)
        lv_obj_remove_flag(m_two_view->search_section, LV_OBJ_FLAG_HIDDEN);

    if (m_two_view->add_btn)
        lv_obj_add_flag(m_two_view->add_btn, LV_OBJ_FLAG_HIDDEN);
    if (m_done_btn)
        lv_obj_add_flag(m_done_btn, LV_OBJ_FLAG_HIDDEN);
    if (m_two_view->cancel_search_btn)
        lv_obj_remove_flag(m_two_view->cancel_search_btn, LV_OBJ_FLAG_HIDDEN);

    lv_group_t *const group = lv_group_get_default();
    if (group && m_query_ta) {
        lv_group_add_obj(group, m_query_ta);
        lv_group_focus_obj(m_query_ta);
    }

    set_keyboard_visible(true);
}

void StationSearchScreen::exit_search_mode()
{
    if (!m_two_view || !m_two_view->in_search_mode)
        return;

    m_two_view->in_search_mode = false;

    if (m_search_timer) {
        lv_timer_delete(m_search_timer);
        m_search_timer = nullptr;
    }

    set_keyboard_visible(false);
    m_on_search_cancel();
    hide_loading();
    clear_results();

    if (m_query_ta)
        lv_textarea_set_text(m_query_ta, "");

    lv_group_t *const group = lv_group_get_default();
    if (group && m_query_ta) {
        lv_group_remove_obj(m_query_ta);
    }

    if (m_two_view->search_section)
        lv_obj_add_flag(m_two_view->search_section, LV_OBJ_FLAG_HIDDEN);
    if (m_two_view->configured_section)
        lv_obj_remove_flag(m_two_view->configured_section, LV_OBJ_FLAG_HIDDEN);

    if (m_two_view->add_btn)
        lv_obj_remove_flag(m_two_view->add_btn, LV_OBJ_FLAG_HIDDEN);
    if (m_done_btn)
        lv_obj_remove_flag(m_done_btn, LV_OBJ_FLAG_HIDDEN);
    if (m_two_view->cancel_search_btn)
        lv_obj_add_flag(m_two_view->cancel_search_btn, LV_OBJ_FLAG_HIDDEN);
}

// ===== Constructor and Destructor =====

StationSearchScreen::StationSearchScreen(
    std::function<void(const std::string &query)> on_search_request, std::function<void()> on_search_cancel,
    std::function<void(const std::vector<StationResult> &stations)> on_selection_done, std::function<void()> on_back,
    const std::vector<StationResult> &initial_selection, bool show_back_button)
    : m_hide_keyboard([this]() { set_keyboard_visible(false); }), m_on_search_request(std::move(on_search_request)),
      m_on_search_cancel(std::move(on_search_cancel)), m_on_selection_done(std::move(on_selection_done)),
      m_on_back(std::move(on_back))
{
    m_screen = lv_obj_create(NULL);
    ui_set_test_id(m_screen, "station_search.screen");
    ui_style_screen(m_screen);
    lv_obj_clear_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(m_screen, generic_callback_cb, LV_EVENT_CLICKED, &m_hide_keyboard);

    build_content_container();
    build_header(show_back_button);
    build_configured_list();
    build_search_input();
    build_results_list();
    build_keyboard();

    for (const auto &station : initial_selection) {
        m_station_by_id[station.id] = station;
        set_station_selected(station.id, station.name, true);
    }
    update_configured_empty_state();
    update_configured_header();
    update_done_button_state();

    if (!m_two_view) {
        set_keyboard_visible(true);
    }
}

void StationSearchScreen::build_content_container()
{
    // Calculate keyboard height from natural key proportions and cap it so the
    // content area still has room. On narrow portrait displays (480px wide),
    // enforce a higher minimum so keys stay comfortably tappable.
    lv_display_t *disp = lv_display_get_default();
    int32_t h_res = lv_display_get_horizontal_resolution(disp);
    int32_t v_res = lv_display_get_vertical_resolution(disp);
    int32_t kb_height_natural = h_res / 2;      // Natural aspect ratio (2:1)
    int32_t kb_height_max = (v_res * 45) / 100; // Cap at 45% of screen height
    m_kb_height = (kb_height_natural < kb_height_max) ? kb_height_natural : kb_height_max;

    bool is_landscape = h_res > v_res;
    bool use_two_view_mode = v_res <= 600;
    if (is_landscape && use_two_view_mode) {
        m_two_view = std::make_unique<TwoViewModeUi>(this);
    }
    if (!is_landscape && h_res <= 500) {
        constexpr int32_t min_narrow_portrait_kb_height = 300;
        if (m_kb_height < min_narrow_portrait_kb_height) {
            m_kb_height = min_narrow_portrait_kb_height;
        }
        if (m_kb_height > kb_height_max) {
            m_kb_height = kb_height_max;
        }
    }

    m_content = lv_obj_create(m_screen);
    lv_obj_remove_style_all(m_content);
    lv_obj_set_size(m_content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(m_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_content, UI_PADDING, 0);
    lv_obj_set_style_pad_bottom(m_content, m_two_view ? 0 : m_kb_height, 0);
    lv_obj_set_style_pad_gap(m_content, 10, 0);
    lv_obj_clear_flag(m_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(m_content, LV_OBJ_FLAG_CLICKABLE);
}

void StationSearchScreen::build_header(bool show_back_button)
{
    lv_obj_t *header_row = lv_obj_create(m_content);
    lv_obj_remove_style_all(header_row);
    lv_obj_set_width(header_row, LV_PCT(100));
    lv_obj_set_height(header_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header_row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *left_group = lv_obj_create(header_row);
    lv_obj_remove_style_all(left_group);
    lv_obj_set_width(left_group, LV_SIZE_CONTENT);
    lv_obj_set_height(left_group, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(left_group, 12, 0);
    lv_obj_clear_flag(left_group, LV_OBJ_FLAG_CLICKABLE);

    if (show_back_button) {
        lv_obj_t *back_btn = ui_create_button(left_group, "Back", ScreenBase::generic_callback_cb, &m_on_back);
        ui_set_test_id(back_btn, "station_search.back_button");
    }

    lv_obj_t *title = lv_label_create(left_group);
    lv_label_set_text(title, show_back_button ? "Manage Stations" : "Select Stations");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_TITLE, 0);

    lv_obj_t *done_parent = header_row;
    if (m_two_view) {
        lv_obj_t *right_group = lv_obj_create(header_row);
        lv_obj_remove_style_all(right_group);
        lv_obj_set_width(right_group, LV_SIZE_CONTENT);
        lv_obj_set_height(right_group, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(right_group, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(right_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(right_group, 8, 0);
        lv_obj_clear_flag(right_group, LV_OBJ_FLAG_CLICKABLE);

        m_two_view->cancel_search_btn =
            ui_create_button(right_group, "Cancel", ScreenBase::generic_callback_cb, &m_two_view->on_cancel_search);
        ui_set_test_id(m_two_view->cancel_search_btn, "station_search.cancel_button");
        lv_obj_add_flag(m_two_view->cancel_search_btn, LV_OBJ_FLAG_HIDDEN);

        m_two_view->add_btn =
            ui_create_button(right_group, "Add", ScreenBase::generic_callback_cb, &m_two_view->on_add_station);
        ui_set_test_id(m_two_view->add_btn, "station_search.add_button");
        done_parent = right_group;
    }

    m_done_btn = ui_btn_create(done_parent);
    ui_set_test_id(m_done_btn, "station_search.done_button");
    lv_obj_set_style_bg_color(m_done_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(m_done_btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(m_done_btn, UI_COLOR_BUTTON_DISABLED_BG, LV_STATE_DISABLED);
    lv_obj_set_style_pad_left(m_done_btn, 14, 0);
    lv_obj_set_style_pad_right(m_done_btn, 14, 0);
    lv_obj_set_style_pad_top(m_done_btn, 10, 0);
    lv_obj_set_style_pad_bottom(m_done_btn, 10, 0);
    lv_obj_add_event_cb(m_done_btn, done_button_cb, LV_EVENT_CLICKED, this);

    m_done_label = lv_label_create(m_done_btn);
    lv_label_set_text(m_done_label, show_back_button ? "Save" : "Continue");
    lv_obj_set_style_text_color(m_done_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(m_done_label, FONT_BUTTON, 0);
    lv_obj_center(m_done_label);
}

void StationSearchScreen::build_search_input()
{
    lv_obj_t *query_parent = m_content;
    if (m_two_view) {
        m_two_view->search_section = lv_obj_create(m_content);
        lv_obj_remove_style_all(m_two_view->search_section);
        lv_obj_set_width(m_two_view->search_section, LV_PCT(100));
        lv_obj_set_height(m_two_view->search_section, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(m_two_view->search_section, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_grow(m_two_view->search_section, 1);
        lv_obj_set_style_pad_gap(m_two_view->search_section, 8, 0);
        lv_obj_clear_flag(m_two_view->search_section, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(m_two_view->search_section, LV_OBJ_FLAG_HIDDEN);
        query_parent = m_two_view->search_section;
    }

    m_query_ta = ui_textarea_create(query_parent);
    ui_set_test_id(m_query_ta, "station_search.search_input");
    auto placeholder = std::format("Type station name (min {} chars)...", STATION_SEARCH_MIN_QUERY_CHARS);
    lv_textarea_set_placeholder_text(m_query_ta, placeholder.c_str());
    lv_textarea_set_one_line(m_query_ta, true);
    lv_obj_set_width(m_query_ta, LV_PCT(100));
    lv_obj_set_style_bg_color(m_query_ta, UI_COLOR_CARD, 0);
    lv_obj_set_style_text_color(m_query_ta, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(m_query_ta, FONT_INPUT, 0);
    lv_obj_set_style_border_color(m_query_ta, UI_COLOR_PRIMARY, 0);

    // Limit max characters so text doesn't extend behind the spinner.
    // Textarea width ≈ display width - 2*UI_PADDING - textarea internal padding (~20).
    // Reserve 40px for the spinner area.
    lv_display_t *disp = lv_display_get_default();
    int32_t h_res = lv_display_get_horizontal_resolution(disp);
    int32_t ta_text_width = h_res - 2 * UI_PADDING - 20 - 40;
    size_t max_chars = ui_max_repeated_text_count_for_width_px(FONT_INPUT, "W", ta_text_width);
    if (max_chars > 0) {
        lv_textarea_set_max_length(m_query_ta, static_cast<uint32_t>(max_chars));
    }

    lv_obj_add_event_cb(m_query_ta, ta_event_cb, LV_EVENT_FOCUSED, this);
    lv_obj_add_event_cb(m_query_ta, ta_event_cb, LV_EVENT_DEFOCUSED, this);
    lv_obj_add_event_cb(m_query_ta, ta_event_cb, LV_EVENT_VALUE_CHANGED, this);

    if (m_two_view && m_two_view->search_section) {
        lv_obj_t *search_hint_label = lv_label_create(m_two_view->search_section);
        lv_label_set_text(search_hint_label, "Tap on a station to add it");
        lv_obj_set_style_text_color(search_hint_label, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(search_hint_label, FONT_SMALL, 0);
        lv_obj_set_width(search_hint_label, LV_PCT(100));
        lv_label_set_long_mode(search_hint_label, LV_LABEL_LONG_WRAP);
    }
}

void StationSearchScreen::build_configured_list()
{
    lv_obj_t *configured_section = lv_obj_create(m_content);
    lv_obj_remove_style_all(configured_section);
    lv_obj_set_width(configured_section, LV_PCT(100));
    lv_obj_set_height(configured_section, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(configured_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(configured_section, 6, 0);
    lv_obj_clear_flag(configured_section, LV_OBJ_FLAG_CLICKABLE);
    if (m_two_view) {
        m_two_view->configured_section = configured_section;
        lv_obj_set_flex_grow(configured_section, 1);
    }

    m_configured_title_label = lv_label_create(configured_section);
    lv_label_set_text(m_configured_title_label, "Configured Stations");
    lv_obj_set_style_text_color(m_configured_title_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(m_configured_title_label, FONT_BODY, 0);

    m_configured_empty_label = lv_label_create(configured_section);
    lv_label_set_text(m_configured_empty_label, "No configured stations yet");
    lv_obj_set_style_text_color(m_configured_empty_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(m_configured_empty_label, FONT_SMALL, 0);
    lv_obj_set_width(m_configured_empty_label, LV_PCT(100));
    lv_label_set_long_mode(m_configured_empty_label, LV_LABEL_LONG_DOT);

    m_configured_list = lv_list_create(configured_section);
    ui_set_test_id(m_configured_list, "station_search.configured_list");
    lv_obj_set_width(m_configured_list, LV_PCT(100));
    if (m_two_view) {
        lv_obj_set_flex_grow(m_configured_list, 1);
    } else {
        lv_display_t *disp = lv_display_get_default();
        int32_t v_res = lv_display_get_vertical_resolution(disp);
        int32_t configured_h = (v_res * 26) / 100;
        if (configured_h < 90) {
            configured_h = 90;
        } else if (configured_h > 170) {
            configured_h = 170;
        }
        lv_obj_set_height(m_configured_list, configured_h);
    }
    lv_obj_set_style_bg_color(m_configured_list, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_configured_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_configured_list, 0, 0);
    lv_obj_set_style_radius(m_configured_list, 0, 0);
    lv_obj_set_style_pad_left(m_configured_list, 0, 0);
    lv_obj_set_style_pad_right(m_configured_list, 12, 0);
    lv_obj_set_style_pad_top(m_configured_list, 4, 0);
    lv_obj_set_style_pad_bottom(m_configured_list, 4, 0);
    lv_obj_set_style_clip_corner(m_configured_list, true, 0);
    lv_obj_set_scroll_dir(m_configured_list, LV_DIR_VER);
    lv_obj_clear_flag(m_configured_list, LV_OBJ_FLAG_CLICKABLE);
}

void StationSearchScreen::build_results_list()
{
    lv_obj_t *results_parent = m_content;
    if (m_two_view && m_two_view->search_section) {
        results_parent = m_two_view->search_section;
    }

    m_results_list = lv_list_create(results_parent);
    ui_set_test_id(m_results_list, "station_search.results_list");
    lv_obj_set_width(m_results_list, LV_PCT(100));
    lv_obj_set_flex_grow(m_results_list, 1);
    lv_obj_set_style_bg_color(m_results_list, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_results_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_results_list, 0, 0);
    lv_obj_set_style_radius(m_results_list, 0, 0);
    lv_obj_set_style_pad_left(m_results_list, 0, 0);
    lv_obj_set_style_pad_right(m_results_list, 12, 0);
    lv_obj_set_style_pad_top(m_results_list, 4, 0);
    lv_obj_set_style_pad_bottom(m_results_list, 4, 0);
    lv_obj_set_style_clip_corner(m_results_list, true, 0);
    lv_obj_set_scroll_dir(m_results_list, LV_DIR_VER);
    lv_obj_clear_flag(m_results_list, LV_OBJ_FLAG_CLICKABLE);
}

void StationSearchScreen::build_keyboard()
{
    lv_display_t *disp = lv_display_get_default();
    int32_t h_res = lv_display_get_horizontal_resolution(disp);
    int32_t v_res = lv_display_get_vertical_resolution(disp);
    int32_t short_edge = std::min(h_res, v_res);
    bool compact_keyboard = short_edge <= 500;

    m_keyboard = lv_keyboard_create(m_screen);
    lv_obj_set_size(m_keyboard, LV_PCT(100), m_kb_height);
    lv_obj_align(m_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    if (compact_keyboard) {
        // Tighter key gaps on the smaller board to increase touch target size.
        lv_obj_set_style_pad_column(m_keyboard, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_row(m_keyboard, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_all(m_keyboard, 4, LV_PART_MAIN);
    }
    lv_obj_set_style_text_font(m_keyboard, FONT_INPUT, LV_PART_ITEMS);
    lv_keyboard_set_textarea(m_keyboard, m_query_ta);
    lv_obj_add_flag(m_keyboard, LV_OBJ_FLAG_HIDDEN);

    // Add textarea to default input group for keyboard input (simulator)
    if (!m_two_view) {
        lv_group_t *const group = lv_group_get_default();
        if (group) {
            lv_group_add_obj(group, m_query_ta);
            lv_group_focus_obj(m_query_ta);
        }
    }
}

StationSearchScreen::~StationSearchScreen()
{
    if (m_search_timer) {
        lv_timer_delete(m_search_timer);
        m_search_timer = nullptr;
    }
    // Remove textarea from input group before deleting
    if (m_query_ta) {
        lv_group_t *const group = lv_group_get_default();
        if (group) {
            lv_group_remove_obj(m_query_ta);
        }
    }
    m_query_ta = nullptr;
    m_configured_list = nullptr;
    m_configured_title_label = nullptr;
    m_configured_empty_label = nullptr;
    m_results_list = nullptr;
    m_keyboard = nullptr;
    m_content = nullptr;
    m_done_btn = nullptr;
    m_done_label = nullptr;
    m_status_item = nullptr;
    m_loading_spinner = nullptr;
    if (m_screen) {
        lv_obj_delete(m_screen);
        m_screen = nullptr;
    }
    m_two_view.reset();
}

bool StationSearchScreen::is_station_selected(const std::string &id) const
{
    return std::find(m_selected_order.begin(), m_selected_order.end(), id) != m_selected_order.end();
}

void StationSearchScreen::apply_station_item_style(lv_obj_t *item)
{
    if (!item)
        return;

    lv_obj_set_style_bg_color(item, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_color(item, UI_COLOR_PRESSED, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(item, 0, 0);
}

void StationSearchScreen::update_configured_empty_state()
{
    if (!m_configured_empty_label)
        return;

    if (m_selected_order.empty()) {
        lv_label_set_text(m_configured_empty_label, "No configured stations yet");
    } else {
        lv_label_set_text(m_configured_empty_label, "Tap station to remove");
    }
}

void StationSearchScreen::update_configured_header()
{
    if (!m_configured_title_label)
        return;

    lv_label_set_text(m_configured_title_label,
                      std::format("Configured Stations ({})", m_selected_order.size()).c_str());
}

void StationSearchScreen::rebuild_pending_order()
{
    m_pending_order.clear();
    for (const auto &id : m_latest_result_order) {
        if (is_station_selected(id))
            continue;
        if (m_result_items.find(id) != m_result_items.end()) {
            m_pending_order.push_back(id);
        }
    }
}

void StationSearchScreen::create_configured_item(const StationResult &sr)
{
    if (!m_configured_list || sr.id.empty()) {
        return;
    }

    if (m_configured_items.find(sr.id) != m_configured_items.end()) {
        return;
    }

    lv_obj_t *item = create_station_item(m_configured_list, sr);
    ui_set_test_id(item, std::format("station_search.configured_item.{}", sr.id));
    m_configured_items[sr.id] = item;
    apply_station_item_style(item);
    update_configured_empty_state();
}

void StationSearchScreen::remove_configured_item(const std::string &id)
{
    auto it = m_configured_items.find(id);
    if (it == m_configured_items.end()) {
        return;
    }

    if (it->second) {
        // Configured rows can be removed from their own event callback.
        lv_obj_delete_async(it->second);
    }
    m_configured_items.erase(it);
    update_configured_empty_state();
}

void StationSearchScreen::update_done_button_state()
{
    const size_t count = m_selected_order.size();
    if (m_two_view && m_two_view->add_btn) {
        if (count >= MAX_CONFIGURED_STATIONS) {
            lv_obj_add_state(m_two_view->add_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(m_two_view->add_btn, LV_STATE_DISABLED);
        }
    }

    if (m_done_btn) {
        if (count == 0) {
            lv_obj_add_state(m_done_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(m_done_btn, LV_STATE_DISABLED);
        }
    }

    if (m_done_label) {
        lv_label_set_text(m_done_label, "Done");
        lv_obj_set_style_text_color(m_done_label, count == 0 ? UI_COLOR_BUTTON_DISABLED_TEXT : UI_COLOR_TEXT, 0);
    }
}

void StationSearchScreen::set_station_selected(const std::string &id, const std::string &name, bool selected)
{
    if (id.empty())
        return;

    if (selected) {
        auto station_it = m_station_by_id.find(id);
        if (station_it == m_station_by_id.end()) {
            StationResult station;
            station.id = id;
            station.name = name.empty() ? id : name;
            m_station_by_id[id] = std::move(station);
        } else if (!name.empty()) {
            station_it->second.name = name;
        }

        if (!is_station_selected(id)) {
            if (m_selected_order.size() >= MAX_CONFIGURED_STATIONS) {
                ui_show_error("Station Limit Reached",
                              std::format("You can select up to {} stations.", MAX_CONFIGURED_STATIONS));
                return;
            }
            m_selected_order.push_back(id);
        }

        station_it = m_station_by_id.find(id);
        if (station_it != m_station_by_id.end()) {
            create_configured_item(station_it->second);
        }

        remove_result_item(id);
    } else {
        m_selected_order.erase(std::remove(m_selected_order.begin(), m_selected_order.end(), id),
                               m_selected_order.end());
        remove_configured_item(id);

        if (m_latest_result_ids.find(id) != m_latest_result_ids.end()) {
            auto station_it = m_station_by_id.find(id);
            if (station_it != m_station_by_id.end() && m_result_items.find(id) == m_result_items.end()) {
                create_result_item(station_it->second);
            }
        } else {
            remove_result_item(id);
            m_station_by_id.erase(id);
        }
    }

    update_configured_empty_state();
    update_configured_header();
    rebuild_pending_order();
    finish_update();
    update_done_button_state();
}

std::vector<StationResult> StationSearchScreen::get_selected_stations() const
{
    std::vector<StationResult> selected;
    selected.reserve(m_selected_order.size());

    for (const std::string &id : m_selected_order) {
        auto it = m_station_by_id.find(id);
        if (it != m_station_by_id.end()) {
            selected.push_back(it->second);
        }
    }

    return selected;
}

void StationSearchScreen::clear_results()
{
    if (!m_results_list)
        return;

    m_latest_result_ids.clear();
    m_latest_result_order.clear();
    clear_status_item();

    // Collect keys first since remove_result_item erases from m_result_items.
    std::vector<std::string> to_remove;
    to_remove.reserve(m_result_items.size());
    std::transform(m_result_items.begin(), m_result_items.end(), std::back_inserter(to_remove),
                   [](const auto &pair) { return pair.first; });
    for (const auto &id : to_remove) {
        remove_result_item(id);
    }

    m_pending_order.clear();
}

std::unordered_set<std::string> StationSearchScreen::begin_update(const std::vector<StationResult> &results)
{
    // Build set of latest station IDs and record desired order for unconfigured results.
    std::unordered_set<std::string> new_ids;
    std::unordered_set<std::string> to_create;
    m_pending_order.clear();
    m_latest_result_order.clear();
    clear_status_item();

    for (const auto &sr : results) {
        m_station_by_id[sr.id] = sr;
        new_ids.insert(sr.id);
        m_latest_result_order.push_back(sr.id);

        // Configured stations are shown only in the configured section.
        if (is_station_selected(sr.id)) {
            continue;
        }

        m_pending_order.push_back(sr.id);
        if (m_result_items.find(sr.id) == m_result_items.end()) {
            to_create.insert(sr.id);
        }
    }
    m_latest_result_ids = new_ids;

    // Remove stale items and hide any items that are now configured.
    std::vector<std::string> to_remove;
    for (const auto &pair : m_result_items) {
        const std::string &id = pair.first;
        if (new_ids.find(id) == new_ids.end() || is_station_selected(id)) {
            to_remove.push_back(id);
        }
    }
    for (const auto &id : to_remove) {
        remove_result_item(id);
    }

    ESP_LOGI(TAG, "Search update: %d results, %d new, %d removed", (int)results.size(), (int)to_create.size(),
             (int)to_remove.size());

    return to_create;
}

lv_obj_t *StationSearchScreen::create_station_item(lv_obj_t *parent, const StationResult &sr)
{
    if (!parent || sr.id.empty()) {
        return nullptr;
    }

    // Clickable item container with horizontal layout
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_width(item, LV_PCT(100));
    lv_obj_set_height(item, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW); // Horizontal layout
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(item, 10, 0);
    lv_obj_set_style_pad_gap(item, 10, 0);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    // Create context for this result item and bind its lifetime to the row object.
    auto *ctx = ui_manage_callback_context(item, new ResultItemContext{this, sr.id, sr.name});
    if (!ctx) {
        lv_obj_delete(item);
        return nullptr;
    }
    lv_obj_add_event_cb(item, result_item_cb, LV_EVENT_ALL, ctx);

    // Station name (left column, 40% width)
    lv_obj_t *name_label = lv_label_create(item);
    lv_label_set_text(name_label, sr.name.c_str());
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(name_label, LV_PCT(40));
    lv_obj_set_style_text_color(name_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(name_label, FONT_INPUT, 0);
    lv_obj_clear_flag(name_label, LV_OBJ_FLAG_CLICKABLE);

    // Line badges container (right column, 60% width)
    ui_create_station_badges(item, sr.lines, LV_PCT(60), false);

    // Custom row divider (not border-based) anchored at the bottom.
    lv_obj_update_layout(item);
    int32_t item_width = lv_obj_get_width(item);
    int32_t pad_left = static_cast<int32_t>(lv_obj_get_style_pad_left(item, LV_PART_MAIN));
    lv_obj_t *divider = lv_obj_create(item);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, item_width, 1);
    int32_t divider_offset_y = static_cast<int32_t>(lv_obj_get_style_pad_bottom(item, LV_PART_MAIN));
    lv_obj_align(divider, LV_ALIGN_BOTTOM_LEFT, -pad_left, divider_offset_y);
    lv_obj_add_flag(divider, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(divider, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_move_foreground(divider);

    return item;
}

void StationSearchScreen::create_result_item(const StationResult &sr)
{
    if (!m_results_list || sr.id.empty() || is_station_selected(sr.id))
        return;

    m_station_by_id[sr.id] = sr;
    lv_obj_t *item = create_station_item(m_results_list, sr);
    if (!item) {
        return;
    }

    ui_set_test_id(item, std::format("station_search.result_item.{}", sr.id));
    m_result_items[sr.id] = item;
    apply_station_item_style(item);
}

void StationSearchScreen::finish_update()
{
    if (!m_results_list)
        return;

    // Reorder LVGL objects to match the desired order
    for (size_t i = 0; i < m_pending_order.size(); i++) {
        auto it = m_result_items.find(m_pending_order[i]);
        if (it != m_result_items.end() && it->second) {
            lv_obj_move_to_index(it->second, i);
        }
    }
}

void StationSearchScreen::remove_result_item(const std::string &id)
{
    auto it = m_result_items.find(id);
    if (it == m_result_items.end()) {
        return;
    }

    if (it->second) {
        // Search result rows can be removed while LVGL is still dispatching
        // their release event, so defer actual destruction to the next tick.
        lv_obj_delete_async(it->second);
    }
    m_result_items.erase(it);
    m_pending_order.erase(std::remove(m_pending_order.begin(), m_pending_order.end(), id), m_pending_order.end());
    if (!is_station_selected(id)) {
        m_station_by_id.erase(id);
    }
}

void StationSearchScreen::clear_status_item()
{
    if (!m_status_item) {
        return;
    }

    lv_obj_delete(m_status_item);
    m_status_item = nullptr;
}

void StationSearchScreen::show_error(const char *message)
{
    if (!m_results_list)
        return;

    clear_status_item();

    // Create error container
    lv_obj_t *error_container = lv_obj_create(m_results_list);
    m_status_item = error_container;
    lv_obj_remove_style_all(error_container);
    lv_obj_set_width(error_container, LV_PCT(100));
    lv_obj_set_height(error_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(error_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(error_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(error_container, 20, 0);
    lv_obj_set_style_pad_gap(error_container, 10, 0);

    // Error symbol (X)
    lv_obj_t *symbol = lv_label_create(error_container);
    lv_label_set_text(symbol, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(symbol, UI_COLOR_RED, 0);
    lv_obj_set_style_text_font(symbol, FONT_TITLE, 0);

    // Error message
    lv_obj_t *msg_label = lv_label_create(error_container);
    lv_label_set_text(msg_label, message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, LV_PCT(90));
    lv_obj_set_style_text_color(msg_label, UI_COLOR_RED, 0);
    lv_obj_set_style_text_font(msg_label, FONT_BODY, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
}

void StationSearchScreen::show_loading()
{
    if (!m_query_ta)
        return;

    if (!m_loading_spinner) {
        // Create spinner overlaid on the search input, aligned to the right.
        // Parent is m_content (not m_query_ta) to avoid affecting textarea height.
        m_loading_spinner = lv_spinner_create(m_content);
        lv_obj_set_size(m_loading_spinner, 30, 30);
        lv_obj_add_flag(m_loading_spinner, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_add_flag(m_loading_spinner, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_style_arc_width(m_loading_spinner, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_width(m_loading_spinner, 4, LV_PART_INDICATOR);
    }

    // Restart animation in case it was stopped while hidden.
    lv_spinner_set_anim_params(m_loading_spinner, 1000, 200);
    lv_obj_align_to(m_loading_spinner, m_query_ta, LV_ALIGN_OUT_RIGHT_MID, -40, 0);
    lv_obj_clear_flag(m_loading_spinner, LV_OBJ_FLAG_HIDDEN);
}

void StationSearchScreen::hide_loading()
{
    if (m_loading_spinner) {
        // Stop spinner animation while hidden to avoid background CPU use.
        lv_anim_delete(m_loading_spinner, nullptr);
        lv_obj_add_flag(m_loading_spinner, LV_OBJ_FLAG_HIDDEN);
    }
}
