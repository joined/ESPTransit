#include "departures.h"
#include "app_constants.h"
#include "app_platform.h"
#include "ui/common.h"
#include "esp_log.h"
#include <cstring>
#include <ctime>
#include <format>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <algorithm>

static const char *TAG = "ui_dep";
static const char *ACTIVE_TAB_INDICATOR = "*";
static constexpr int32_t LINE_COLUMN_WIDTH = 50;
static constexpr int32_t LINE_BADGE_HEIGHT = 36;
static constexpr int32_t SPLIT_PANEL_RADIUS = 8;

// ===== DepartureItem helper class =====

// DepartureItem class manages a single departure row
class DeparturesScreen::DepartureItem {
  public:
    DepartureItem()
        : m_row(nullptr), m_line_badge(nullptr), m_line_label(nullptr), m_direction_label(nullptr),
          m_time_label(nullptr), m_dep()
    {
    }

    void create(lv_obj_t *parent, const DepartureResult &dep);

    void update(const DepartureResult &dep);

    lv_obj_t *get_row() const { return m_row; }
    time_t get_when_time() const { return m_dep.when_time; }

  private:
    std::string get_formatted_when() const; // Format the time string

    lv_obj_t *m_row;
    lv_obj_t *m_line_badge;
    lv_obj_t *m_line_label;
    lv_obj_t *m_direction_label;
    lv_obj_t *m_time_label;
    DepartureResult m_dep;
};

struct DeparturesScreen::StationSplitView {
    lv_obj_t *panel = nullptr;
    lv_obj_t *list = nullptr;
    lv_obj_t *spinner = nullptr;
    std::unordered_map<std::string, DepartureItem> departure_items;
};

std::string DeparturesScreen::DepartureItem::get_formatted_when() const
{
    if (m_dep.cancelled) {
        return "#" UI_RECOLOR_ERROR_HEX " cancelled#";
    }
    if (m_dep.when_time == 0) {
        return "--";
    }

    // Compute minutes from now
    time_t now = AppPlatform::get_wall_clock_time();
    int when_minutes = static_cast<int>(difftime(m_dep.when_time, now) / 60.0);
    if (when_minutes < 0)
        when_minutes = 0;

    std::string prefix = m_dep.scheduled ? "~" : "";
    std::string base_time;
    if (when_minutes <= 0) {
        base_time = prefix + "now";
    } else if (when_minutes < 60) {
        base_time = prefix + std::to_string(when_minutes) + " min";
    } else {
        base_time = prefix + std::to_string(when_minutes / 60) + "h " + std::to_string(when_minutes % 60) + "m";
    }

    if (m_dep.delay_seconds == 0 || m_dep.scheduled) {
        return base_time;
    }

    int delay_min = m_dep.delay_seconds / 60;
    if (delay_min > 0) {
        return base_time + " #" UI_RECOLOR_ERROR_HEX " (+" + std::to_string(delay_min) + ")#";
    }
    return base_time + " #" UI_RECOLOR_ERROR_HEX " (" + std::to_string(delay_min) + ")#";
}

void DeparturesScreen::DepartureItem::create(lv_obj_t *parent, const DepartureResult &dep)
{
    m_dep = dep;

    // Create row container
    m_row = lv_obj_create(parent);
    lv_obj_remove_style_all(m_row);
    lv_obj_set_width(m_row, LV_PCT(100));
    lv_obj_set_height(m_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(m_row, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_row, 1, 0);
    lv_obj_set_style_border_color(m_row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_side(m_row, LV_BORDER_SIDE_BOTTOM, 0);

    // Inner container with padding for content
    lv_obj_t *content = lv_obj_create(m_row);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(content, 8, 0);
    lv_obj_set_style_pad_left(content, UI_PADDING, 0);
    lv_obj_set_style_pad_right(content, UI_PADDING, 0);
    lv_obj_set_style_pad_gap(content, 12, 0);

    // Line badge
    m_line_badge = lv_obj_create(content);
    lv_obj_remove_style_all(m_line_badge);
    lv_obj_set_size(m_line_badge, LINE_COLUMN_WIDTH, LINE_BADGE_HEIGHT);
    lv_obj_set_style_bg_color(m_line_badge, ui_product_color(dep.line_product), 0);
    lv_obj_set_style_bg_opa(m_line_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(m_line_badge, 5, 0);

    m_line_label = lv_label_create(m_line_badge);
    lv_label_set_text(m_line_label, dep.line_name.c_str());
    lv_obj_set_width(m_line_label, LV_PCT(100));
    lv_label_set_long_mode(m_line_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(m_line_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(m_line_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(m_line_label, FONT_BODY, 0);
    lv_obj_center(m_line_label);

    // Direction label
    m_direction_label = lv_label_create(content);
    lv_label_set_text(m_direction_label, dep.direction.c_str());
    lv_obj_set_flex_grow(m_direction_label, 4);
    lv_obj_set_style_min_width(m_direction_label, 0, 0);
    lv_label_set_long_mode(m_direction_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(m_direction_label, FONT_BODY, 0);
    lv_obj_set_style_text_color(m_direction_label, UI_COLOR_TEXT_DARK, 0);

    // Time label
    m_time_label = lv_label_create(content);
    lv_obj_set_flex_grow(m_time_label, 2);
    lv_obj_set_style_min_width(m_time_label, 0, 0);
    lv_label_set_long_mode(m_time_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(m_time_label, FONT_BODY, 0);
    lv_obj_set_style_text_color(m_time_label, UI_COLOR_TEXT_DARK, 0);
    lv_label_set_recolor(m_time_label, true);

    // Format and set the initial time label
    std::string time_str = get_formatted_when();
    lv_label_set_text(m_time_label, time_str.c_str());
}

void DeparturesScreen::DepartureItem::update(const DepartureResult &dep)
{
    if (!m_row)
        return;

    // Update the stored departure data
    m_dep = dep;

    // Update line and direction too because we may reuse rows after station toggles.
    if (m_line_label) {
        lv_label_set_text(m_line_label, dep.line_name.c_str());
    }
    if (m_line_badge) {
        lv_obj_set_style_bg_color(m_line_badge, ui_product_color(dep.line_product), 0);
    }
    if (m_direction_label) {
        lv_label_set_text(m_direction_label, dep.direction.c_str());
    }

    std::string time_str = get_formatted_when();
    lv_label_set_text(m_time_label, time_str.c_str());
}

void DeparturesScreen::station_tab_changed_cb(lv_event_t *e)
{
    auto *screen = static_cast<DeparturesScreen *>(lv_event_get_user_data(e));
    if (!screen || !screen->m_station_tabs) {
        return;
    }

    uint32_t tab_index = lv_tabview_get_tab_active(screen->m_station_tabs);
    lv_obj_t *button = lv_tabview_get_tab_button(screen->m_station_tabs, static_cast<int32_t>(tab_index));
    if (!button) {
        return;
    }

    const char *station_id = static_cast<const char *>(lv_obj_get_user_data(button));
    if (!station_id || screen->m_active_station_id == station_id) {
        return;
    }

    screen->m_active_station_id = station_id;
    screen->apply_station_tab_layout();
    screen->update_active_station_label();
    if (screen->m_departures_by_station.empty() && screen->m_spinner) {
        return;
    }
    screen->render_departures(screen->filtered_departures());
}

// ===== DeparturesScreen implementation =====

void DeparturesScreen::update()
{
    update_clock_label();
}

void DeparturesScreen::update_clock_label()
{
    // Update clock label
    if (m_clock_label) {
        time_t now = AppPlatform::get_wall_clock_time();
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        auto clock_str = std::format("{:02}:{:02}:{:02}", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        lv_label_set_text(m_clock_label, clock_str.c_str());
    }
}

void DeparturesScreen::build_column_headers(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(row, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);

    lv_obj_t *content = lv_obj_create(row);
    lv_obj_remove_style_all(content);
    lv_obj_set_width(content, LV_PCT(100));
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(content, 8, 0);
    lv_obj_set_style_pad_left(content, UI_PADDING, 0);
    lv_obj_set_style_pad_right(content, UI_PADDING, 0);
    lv_obj_set_style_pad_gap(content, 12, 0);

    lv_obj_t *label = lv_label_create(content);
    lv_label_set_text(label, "Line");
    lv_obj_set_size(label, LINE_COLUMN_WIDTH, LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(label, FONT_BODY, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_DIM, 0);

    const char *texts[] = {"Direction", "Time"};
    const int grows[] = {4, 2};

    for (int i = 0; i < 2; i++) {
        lv_obj_t *label = lv_label_create(content);
        lv_label_set_text(label, texts[i]);
        lv_obj_set_flex_grow(label, grows[i]);
        lv_obj_set_style_min_width(label, 0, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(label, FONT_BODY, 0);
        lv_obj_set_style_text_color(label, UI_COLOR_TEXT_DIM, 0);
    }
}

// ===== Constructor and Destructor =====

DeparturesScreen::DeparturesScreen(std::function<void()> on_settings_pressed,
                                   const std::vector<StationResult> &stations, bool split_mode_enabled,
                                   const std::vector<DepartureResult> &initial_departures,
                                   std::optional<std::string> initial_station_id)
    : m_on_settings_pressed(std::move(on_settings_pressed)), m_stations(stations)
{
    m_split_mode_enabled = split_mode_enabled && m_stations.size() >= SPLIT_MODE_MIN_STATIONS &&
                           m_stations.size() <= SPLIT_MODE_MAX_STATIONS;

    if (!m_stations.empty() && !m_split_mode_enabled) {
        m_active_station_id = m_stations[0].id;
        if (initial_station_id.has_value()) {
            for (const auto &s : m_stations) {
                if (s.id == *initial_station_id) {
                    m_active_station_id = *initial_station_id;
                    break;
                }
            }
        }
    }

    m_screen = lv_obj_create(NULL);
    ui_set_test_id(m_screen, "departures.screen");
    ui_style_screen(m_screen);
    lv_obj_set_flex_flow(m_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_top(m_screen, UI_PADDING, 0);
    lv_obj_set_style_pad_bottom(m_screen, UI_PADDING, 0);
    lv_obj_set_style_pad_left(m_screen, 0, 0);
    lv_obj_set_style_pad_right(m_screen, 0, 0);
    lv_obj_set_style_pad_gap(m_screen, 0, 0);

    build_header();
    if (!m_split_mode_enabled) {
        build_station_tabs();
    }
    if (m_split_mode_enabled) {
        build_split_station_views();
    } else {
        build_column_headers(m_screen);
        auto [list, spinner] = build_departure_list(m_screen);
        m_list = list;
        m_spinner = spinner;
    }

    if (!initial_departures.empty()) {
        update_departures(initial_departures);
    }
    update_active_station_label();
    update_clock_label();
}

void DeparturesScreen::build_header()
{
    lv_obj_t *header = lv_obj_create(m_screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(header, UI_PADDING, 0);
    lv_obj_set_style_pad_right(header, UI_PADDING, 0);
    if (m_split_mode_enabled || m_stations.size() <= 1) {
        lv_obj_set_style_margin_bottom(header, 10, 0);
    }
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    m_station_label = lv_label_create(header);
    lv_label_set_text(m_station_label, "--");
    lv_obj_set_style_text_color(m_station_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(m_station_label, FONT_TITLE_BOLD, 0);
    lv_obj_set_flex_grow(m_station_label, 1);
    lv_obj_set_style_min_width(m_station_label, 0, 0);
    lv_label_set_long_mode(m_station_label, LV_LABEL_LONG_DOT);

    // Current time clock
    m_clock_label = lv_label_create(header);
    lv_label_set_text(m_clock_label, "--:--:--");
    lv_obj_set_style_text_color(m_clock_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(m_clock_label, FONT_BODY, 0);
    lv_obj_set_style_pad_right(m_clock_label, 15, 0);

    // Settings gear button
    lv_obj_t *gear_btn = ui_btn_create(header);
    ui_set_test_id(gear_btn, "departures.settings_button");
    lv_obj_set_style_bg_color(gear_btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_pad_all(gear_btn, 10, 0);
    lv_obj_t *gear_label = lv_label_create(gear_btn);
    lv_label_set_text(gear_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gear_label, FONT_TITLE, 0);
    lv_obj_add_event_cb(gear_btn, ScreenBase::generic_callback_cb, LV_EVENT_CLICKED, &m_on_settings_pressed);
}

void DeparturesScreen::build_station_tabs()
{
    if (m_stations.size() <= 1) {
        return;
    }

    m_station_tabs = lv_tabview_create(m_screen);
    lv_obj_set_width(m_station_tabs, LV_PCT(100));
    lv_obj_set_height(m_station_tabs, 46);
    lv_tabview_set_tab_bar_position(m_station_tabs, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(m_station_tabs, 46);
    lv_obj_set_style_pad_all(m_station_tabs, 0, 0);
    lv_obj_set_style_bg_opa(m_station_tabs, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m_station_tabs, 0, 0);
    lv_obj_set_style_margin_top(m_station_tabs, 10, 0);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(m_station_tabs);
    if (tab_bar) {
        lv_obj_set_style_bg_color(tab_bar, UI_COLOR_BG, 0);
        lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(tab_bar, 0, 0);
        lv_obj_set_style_pad_gap(tab_bar, 0, 0);
        lv_obj_set_style_border_width(tab_bar, 0, 0);
    }

    lv_obj_t *content = lv_tabview_get_content(m_station_tabs);
    if (content) {
        lv_obj_add_flag(content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(content, 0);
    }

    for (size_t i = 0; i < m_stations.size(); ++i) {
        lv_obj_t *tab = lv_tabview_add_tab(m_station_tabs, "");
        if (tab) {
            lv_obj_set_style_bg_opa(tab, LV_OPA_TRANSP, 0);
        }
    }

    for (size_t i = 0; i < m_stations.size(); ++i) {
        lv_obj_t *button = lv_tabview_get_tab_button(m_station_tabs, static_cast<int32_t>(i));
        if (!button)
            continue;

        lv_obj_set_user_data(button, (void *)m_stations[i].id.c_str());

        lv_obj_set_style_bg_color(button, UI_COLOR_TAB_BG, 0);
        lv_obj_set_style_bg_color(button, UI_COLOR_TAB_BG_PRESSED, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(button, UI_COLOR_TAB_BG_ACTIVE, LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, UI_COLOR_TAB_BORDER, 0);
        lv_obj_set_style_border_color(button, UI_COLOR_TAB_BORDER_ACTIVE, LV_STATE_CHECKED);
        lv_obj_set_style_radius(button, 0, 0);
        lv_obj_set_style_pad_left(button, 8, 0);
        lv_obj_set_style_pad_right(button, 8, 0);
        lv_obj_set_style_text_color(button, UI_COLOR_TAB_TEXT_INACTIVE, 0);
        lv_obj_set_style_text_color(button, UI_COLOR_TEXT, LV_STATE_CHECKED);
        lv_obj_set_style_text_font(button, FONT_MEDIUM, 0);

        lv_obj_t *label = lv_obj_get_child(button, 0);
        if (label) {
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
            lv_obj_set_width(label, LV_PCT(100));
            lv_obj_set_height(label, lv_font_get_line_height(FONT_MEDIUM));
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        }
    }

    lv_obj_add_event_cb(m_station_tabs, station_tab_changed_cb, LV_EVENT_VALUE_CHANGED, this);
    uint32_t initial_tab_index = 0;
    for (size_t i = 0; i < m_stations.size(); ++i) {
        if (m_stations[i].id == m_active_station_id) {
            initial_tab_index = static_cast<uint32_t>(i);
            break;
        }
    }
    lv_tabview_set_active(m_station_tabs, initial_tab_index, LV_ANIM_OFF);
    apply_station_tab_layout();
}

void DeparturesScreen::build_split_station_views()
{
    m_split_container = lv_obj_create(m_screen);
    lv_obj_remove_style_all(m_split_container);
    lv_obj_set_width(m_split_container, LV_PCT(100));
    lv_obj_set_flex_grow(m_split_container, 1);
    lv_obj_set_style_pad_left(m_split_container, UI_PADDING, 0);
    lv_obj_set_style_pad_right(m_split_container, UI_PADDING, 0);
    lv_obj_set_style_pad_gap(m_split_container, 12, 0);
    bool landscape = ui_is_landscape();
    bool use_two_by_two_landscape = landscape && m_stations.size() == SPLIT_MODE_MAX_STATIONS;
    lv_obj_set_flex_flow(m_split_container, LV_FLEX_FLOW_COLUMN);
    std::vector<lv_obj_t *> split_rows;
    if (landscape) {
        size_t row_count = use_two_by_two_landscape ? 2 : 1;
        split_rows.reserve(row_count);
        for (size_t row_index = 0; row_index < row_count; ++row_index) {
            lv_obj_t *row = lv_obj_create(m_split_container);
            lv_obj_remove_style_all(row);
            lv_obj_set_width(row, LV_PCT(100));
            lv_obj_set_height(row, 0);
            lv_obj_set_flex_grow(row, 1);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_pad_gap(row, 12, 0);
            split_rows.push_back(row);
        }
    }

    int32_t max_title_label_height = lv_font_get_line_height(ui_font_departures_split_title()) * 2;
    std::vector<lv_obj_t *> split_title_rows;
    split_title_rows.reserve(m_stations.size());

    m_split_views.reserve(m_stations.size());
    for (size_t i = 0; i < m_stations.size(); ++i) {
        m_split_views.emplace_back();
        StationSplitView &view = m_split_views.back();

        lv_obj_t *panel_parent = m_split_container;
        if (landscape) {
            panel_parent = split_rows[use_two_by_two_landscape ? (i / 2) : 0];
        }

        view.panel = lv_obj_create(panel_parent);
        lv_obj_remove_style_all(view.panel);
        lv_obj_set_flex_flow(view.panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(view.panel, 0, 0);
        lv_obj_set_style_pad_gap(view.panel, 0, 0);
        lv_obj_set_style_bg_color(view.panel, UI_COLOR_CARD, 0);
        lv_obj_set_style_bg_opa(view.panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(view.panel, 0, 0);

        if (landscape) {
            lv_obj_set_flex_grow(view.panel, 1);
            lv_obj_set_width(view.panel, 0);
            lv_obj_set_height(view.panel, LV_PCT(100));
        } else {
            lv_obj_set_flex_grow(view.panel, 1);
            lv_obj_set_width(view.panel, LV_PCT(100));
            lv_obj_set_height(view.panel, 0);
        }

        lv_obj_t *title_row = lv_obj_create(view.panel);
        lv_obj_remove_style_all(title_row);
        lv_obj_set_width(title_row, LV_PCT(100));
        lv_obj_set_height(title_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(title_row, UI_COLOR_TAB_BG_ACTIVE, 0);
        lv_obj_set_style_bg_opa(title_row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(title_row, 1, 0);
        lv_obj_set_style_border_color(title_row, UI_COLOR_TAB_BORDER_ACTIVE, 0);
        lv_obj_set_style_border_side(title_row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_pad_top(title_row, 10, 0);
        lv_obj_set_style_pad_bottom(title_row, 10, 0);
        lv_obj_set_style_pad_left(title_row, UI_PADDING, 0);
        lv_obj_set_style_pad_right(title_row, UI_PADDING, 0);
        split_title_rows.push_back(title_row);

        lv_obj_t *title_label = lv_label_create(title_row);
        std::string station_name = m_stations[i].name.empty() ? m_stations[i].id : m_stations[i].name;
        lv_label_set_text(title_label, station_name.c_str());
        lv_obj_set_width(title_label, LV_PCT(100));
        lv_obj_set_style_min_width(title_label, 0, 0);
        lv_label_set_long_mode(title_label, landscape ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_DOT);
        if (landscape) {
            lv_obj_set_style_max_height(title_label, max_title_label_height, 0);
        }
        lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT, 0);
        lv_obj_set_style_text_font(title_label, ui_font_departures_split_title(), 0);

        build_column_headers(view.panel);
        auto [list, spinner] = build_departure_list(view.panel);
        view.list = list;
        view.spinner = spinner;
    }

    if (landscape && !split_title_rows.empty()) {
        lv_obj_update_layout(m_split_container);

        int32_t unified_title_row_height = 0;
        for (lv_obj_t *title_row : split_title_rows) {
            unified_title_row_height = std::max(unified_title_row_height, lv_obj_get_height(title_row));
        }

        int32_t pad_top = lv_obj_get_style_pad_top(split_title_rows.front(), LV_PART_MAIN);
        int32_t pad_bottom = lv_obj_get_style_pad_bottom(split_title_rows.front(), LV_PART_MAIN);
        int32_t min_title_row_height = lv_font_get_line_height(ui_font_departures_split_title()) + pad_top + pad_bottom;
        int32_t max_title_row_height = max_title_label_height + pad_top + pad_bottom;
        if (unified_title_row_height < min_title_row_height) {
            unified_title_row_height = min_title_row_height;
        }
        if (unified_title_row_height > max_title_row_height) {
            unified_title_row_height = max_title_row_height;
        }

        for (lv_obj_t *title_row : split_title_rows) {
            lv_obj_set_height(title_row, unified_title_row_height);
        }
    }
}

void DeparturesScreen::apply_station_tab_layout()
{
    if (!m_station_tabs || m_stations.size() <= 1) {
        return;
    }

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(m_station_tabs);
    if (!tab_bar) {
        return;
    }

    lv_obj_update_layout(m_station_tabs);
    int32_t bar_width = lv_obj_get_content_width(tab_bar);
    size_t station_count = m_stations.size();
    if (bar_width <= 0 || station_count <= 1) {
        return;
    }

    int32_t count_i32 = static_cast<int32_t>(station_count);
    int32_t equal_width = bar_width / count_i32;
    int32_t active_width = equal_width / 2;
    if (active_width < 36) {
        active_width = 36;
    }
    if (active_width > 72) {
        active_width = 72;
    }
    if (active_width > equal_width) {
        active_width = equal_width;
    }

    int32_t remaining_width = bar_width - active_width;
    int32_t inactive_count = count_i32 - 1;
    int32_t inactive_width = remaining_width / inactive_count;
    int32_t remainder = remaining_width % inactive_count;

    for (size_t i = 0; i < station_count; ++i) {
        lv_obj_t *button = lv_tabview_get_tab_button(m_station_tabs, static_cast<int32_t>(i));
        if (!button) {
            continue;
        }

        int32_t width = 0;
        if (m_stations[i].id == m_active_station_id) {
            width = active_width;
        } else {
            width = inactive_width;
            if (remainder > 0) {
                width += 1;
                remainder -= 1;
            }
        }

        lv_obj_set_flex_grow(button, 0);
        lv_obj_set_width(button, width);
    }

    for (size_t i = 0; i < station_count; ++i) {
        const char *title;
        if (m_stations[i].id == m_active_station_id) {
            title = ACTIVE_TAB_INDICATOR;
        } else {
            title = m_stations[i].name.empty() ? m_stations[i].id.c_str() : m_stations[i].name.c_str();
        }
        lv_tabview_rename_tab(m_station_tabs, static_cast<uint32_t>(i), title);
    }
}

void DeparturesScreen::update_active_station_label()
{
    if (!m_station_label)
        return;

    if (m_stations.empty()) {
        lv_label_set_text(m_station_label, "No stations configured");
        return;
    }

    if (m_split_mode_enabled) {
        lv_label_set_text(m_station_label, "ESPTransit");
        return;
    }

    for (const auto &s : m_stations) {
        if (s.id == m_active_station_id) {
            std::string label = s.name.empty() ? s.id : s.name;
            lv_label_set_text(m_station_label, label.c_str());
            return;
        }
    }
}

std::vector<DepartureResult> DeparturesScreen::filtered_departures() const
{
    return filtered_departures(m_active_station_id);
}

std::vector<DepartureResult> DeparturesScreen::filtered_departures(const std::string &station_id) const
{
    auto it = m_departures_by_station.find(station_id);
    return it != m_departures_by_station.end() ? it->second : std::vector<DepartureResult>{};
}

DeparturesScreen::DepartureListWidgets DeparturesScreen::build_departure_list(lv_obj_t *parent)
{
    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(list, 0, 0);
    lv_obj_set_style_bg_color(list, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);

    // Loading spinner (will be removed on first data update)
    lv_obj_t *spinner_container = lv_obj_create(list);
    lv_obj_remove_style_all(spinner_container);
    lv_obj_set_width(spinner_container, LV_PCT(100));
    lv_obj_set_height(spinner_container, 200);
    lv_obj_set_style_bg_opa(spinner_container, LV_OPA_TRANSP, 0);

    lv_obj_t *spinner = lv_spinner_create(spinner_container);
    lv_spinner_set_anim_params(spinner, 1000, 200);
    lv_obj_set_size(spinner, 60, 60);
    lv_obj_center(spinner);

    return {list, spinner_container};
}

DeparturesScreen::~DeparturesScreen()
{
    m_departure_items.clear();
    for (auto &view : m_split_views) {
        view.departure_items.clear();
        view.panel = nullptr;
        view.list = nullptr;
        view.spinner = nullptr;
    }
    m_split_views.clear();
    m_list = nullptr;
    m_spinner = nullptr;
    m_station_tabs = nullptr;
    m_split_container = nullptr;
    m_station_label = nullptr;
    m_clock_label = nullptr;
    if (m_screen) {
        lv_obj_delete(m_screen);
        m_screen = nullptr;
    }
}

void DeparturesScreen::update_departures(const std::vector<DepartureResult> &departures)
{
    if (!m_list && m_split_views.empty())
        return;

    m_departures_by_station.clear();
    for (const auto &dep : departures) {
        const std::string &stop_id = dep.stop_id;
        if (stop_id.empty()) {
            if (m_stations.size() == 1) {
                m_departures_by_station[m_stations[0].id].push_back(dep);
            }
        } else {
            m_departures_by_station[stop_id].push_back(dep);
        }
    }

    if (m_split_mode_enabled) {
        render_departures({});
    } else {
        render_departures(filtered_departures());
    }
}

void DeparturesScreen::render_departures(const std::vector<DepartureResult> &departures)
{
    if (m_split_mode_enabled) {
        for (size_t i = 0; i < m_split_views.size() && i < m_stations.size(); ++i) {
            std::vector<DepartureResult> station_departures = filtered_departures(m_stations[i].id);
            render_departures_for_list(m_split_views[i].list, m_split_views[i].spinner,
                                       m_split_views[i].departure_items, station_departures);
        }
        return;
    }

    render_departures_for_list(m_list, m_spinner, m_departure_items, departures);
}

void DeparturesScreen::render_departures_for_list(lv_obj_t *list, lv_obj_t *&spinner,
                                                  std::unordered_map<std::string, DepartureItem> &departure_items,
                                                  const std::vector<DepartureResult> &departures)
{
    if (!list) {
        return;
    }

    // Remove spinner if it exists (first data load)
    if (spinner) {
        lv_obj_delete(spinner);
        spinner = nullptr;
    }

    // Track current trip IDs for reconciliation
    std::unordered_set<std::string> current_trip_ids;

    // Step 1: Update existing items or create new ones
    for (const DepartureResult &dep : departures) {
        // Skip entries without tripId (shouldn't happen, but be defensive)
        if (dep.trip_id.empty())
            continue;

        const std::string &trip_id = dep.trip_id;
        current_trip_ids.insert(trip_id);

        auto it = departure_items.find(trip_id);
        if (it != departure_items.end()) {
            // Update existing item
            it->second.update(dep);
        } else {
            // Create new item
            DepartureItem new_item;
            new_item.create(list, dep);
            departure_items[trip_id] = new_item;
        }
    }

    // Step 2: Remove stale items (trips no longer in the API response)
    std::vector<std::string> items_to_remove;
    for (const auto &pair : departure_items) {
        if (current_trip_ids.find(pair.first) == current_trip_ids.end()) {
            items_to_remove.push_back(pair.first);
        }
    }
    for (const auto &trip_id : items_to_remove) {
        auto it = departure_items.find(trip_id);
        if (it != departure_items.end()) {
            lv_obj_t *row = it->second.get_row();
            if (row) {
                lv_obj_delete(row);
            }
            departure_items.erase(it);
        }
    }

    // Step 3: Reorder items by departure time (when_time ascending)
    // Build a sorted list of tripIds based on when_time
    std::vector<std::pair<std::string, time_t>> sorted_items;
    for (const auto &pair : departure_items) {
        sorted_items.push_back({pair.first, pair.second.get_when_time()});
    }
    std::sort(sorted_items.begin(), sorted_items.end(), [](const auto &a, const auto &b) {
        // Sort by when_time ascending (soonest first)
        // Cancelled items (when_time == 0) go to the end
        if (a.second == 0 && b.second == 0)
            return false;
        if (a.second == 0)
            return false;
        if (b.second == 0)
            return true;
        return a.second < b.second;
    });

    // Reorder LVGL objects to match sorted order
    for (size_t i = 0; i < sorted_items.size(); i++) {
        const std::string &trip_id = sorted_items[i].first;
        auto it = departure_items.find(trip_id);
        if (it != departure_items.end()) {
            lv_obj_t *row = it->second.get_row();
            if (row) {
                lv_obj_move_to_index(row, i);
            }
        }
    }

    // Handle empty state
    if (departure_items.empty()) {
        // Create a placeholder row
        DepartureResult empty_dep;
        empty_dep.direction = "No departures found";
        DepartureItem empty_item;
        empty_item.create(list, empty_dep);
        departure_items["__empty__"] = empty_item;
    } else {
        // Remove empty placeholder if it exists
        auto empty_it = departure_items.find("__empty__");
        if (empty_it != departure_items.end()) {
            lv_obj_t *row = empty_it->second.get_row();
            if (row) {
                lv_obj_delete(row);
            }
            departure_items.erase(empty_it);
        }
    }

    ESP_LOGI(TAG, "Updated departure table: %d current, %d removed", (int)current_trip_ids.size(),
             (int)items_to_remove.size());
}
