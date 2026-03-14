#pragma once

#include "screen_base.h"
#include "lvgl.h"
#include "http_client.h"
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

// Station search screen class - instance-based
class StationSearchScreen : public ScreenBase {
  public:
    StationSearchScreen(std::function<void(const std::string &query)> on_search_request,
                        std::function<void()> on_search_cancel,
                        std::function<void(const std::vector<StationResult> &stations)> on_selection_done,
                        std::function<void()> on_back, const std::vector<StationResult> &initial_selection = {},
                        bool show_back_button = false);

    // Destructor - cleanup
    ~StationSearchScreen() override;

    // Get the LVGL screen object
    lv_obj_t *get_screen() const override { return m_screen; }

    // Diff-based result update. Returns the number of newly created items
    // (caller should yield between lock/unlock cycles for each new item).
    // Call create_result_item() for each new station, under LVGL lock.
    // Returns the set of station IDs that need new LVGL items
    std::unordered_set<std::string> begin_update(const std::vector<StationResult> &results);
    void create_result_item(const StationResult &sr);
    void finish_update();

    // Clear all results (e.g. when input is too short)
    void clear_results();

    // Show search error (call under LVGL lock)
    void show_error(const char *message);

    // Show loading spinner in results area (call under LVGL lock)
    void show_loading();

    // Hide loading spinner (call under LVGL lock)
    void hide_loading();

    // Minimal contexts for callbacks
    struct ResultItemContext {
        StationSearchScreen *screen;
        std::string id;
        std::string name;
    };

  private:
    // Non-copyable
    StationSearchScreen(const StationSearchScreen &) = delete;
    StationSearchScreen &operator=(const StationSearchScreen &) = delete;

    // Static callbacks for LVGL
    static void result_item_cb(lv_event_t *e);
    static void ta_event_cb(lv_event_t *e);
    static void search_timer_cb(lv_timer_t *timer);
    static void done_button_cb(lv_event_t *e);

    struct TwoViewModeUi {
        bool in_search_mode = false;
        lv_obj_t *configured_section = nullptr;
        lv_obj_t *search_section = nullptr;
        lv_obj_t *add_btn = nullptr;
        lv_obj_t *cancel_search_btn = nullptr;
        std::function<void()> on_add_station;
        std::function<void()> on_cancel_search;

        explicit TwoViewModeUi(StationSearchScreen *screen)
            : on_add_station([screen]() { screen->enter_search_mode(); }),
              on_cancel_search([screen]() { screen->exit_search_mode(); })
        {
        }
    };

    // Helper methods
    void set_keyboard_visible(bool show);
    void enter_search_mode();
    void exit_search_mode();
    void build_content_container();
    void build_header(bool show_back_button);
    void build_configured_list();
    void build_search_input();
    void build_results_list();
    void build_keyboard();
    void set_station_selected(const std::string &id, const std::string &name, bool selected);
    bool is_station_selected(const std::string &id) const;
    void apply_station_item_style(lv_obj_t *item);
    lv_obj_t *create_station_item(lv_obj_t *parent, const StationResult &sr);
    void create_configured_item(const StationResult &sr);
    void remove_configured_item(const std::string &id);
    void update_configured_header();
    void update_configured_empty_state();
    void rebuild_pending_order();
    void update_done_button_state();
    std::vector<StationResult> get_selected_stations() const;
    void remove_result_item(const std::string &id);
    void clear_status_item();

    // Member variables
    lv_obj_t *m_screen = nullptr;
    lv_obj_t *m_query_ta = nullptr;
    lv_obj_t *m_configured_list = nullptr;
    lv_obj_t *m_configured_title_label = nullptr;
    lv_obj_t *m_configured_empty_label = nullptr;
    lv_obj_t *m_results_list = nullptr;
    lv_obj_t *m_keyboard = nullptr;
    lv_obj_t *m_loading_spinner = nullptr;
    lv_obj_t *m_content = nullptr; // Content container (to adjust padding when keyboard shows/hides)
    lv_obj_t *m_done_btn = nullptr;
    lv_obj_t *m_done_label = nullptr;
    lv_obj_t *m_status_item = nullptr;
    lv_timer_t *m_search_timer = nullptr;
    int32_t m_kb_height = 0; // Keyboard height for dynamic padding adjustment
    std::unique_ptr<TwoViewModeUi> m_two_view;
    std::function<void()> m_hide_keyboard;
    std::function<void(const std::string &)> m_on_search_request;
    std::function<void()> m_on_search_cancel;
    std::function<void(const std::vector<StationResult> &stations)> m_on_selection_done;
    std::function<void()> m_on_back;
    std::unordered_map<std::string, StationResult> m_station_by_id; // Latest known metadata for each station id
    std::vector<std::string> m_selected_order;                      // Selected station IDs in insertion order
    std::unordered_map<std::string, lv_obj_t *> m_configured_items; // station id -> configured list item

    // Diff-based result tracking
    std::unordered_map<std::string, lv_obj_t *> m_result_items; // station id -> search result item
    std::vector<std::string> m_pending_order;                   // desired order for finish_update
    std::vector<std::string> m_latest_result_order;             // station IDs in latest API order
    std::unordered_set<std::string> m_latest_result_ids;        // station ids from latest search response
};
