#pragma once

#include "screen_base.h"
#include "lvgl.h"
#include "http_client.h"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Departures screen class - instance-based
class DeparturesScreen : public ScreenBase {
  public:
    DeparturesScreen(std::function<void()> on_settings_pressed, const std::vector<StationResult> &stations,
                     bool split_mode_enabled, const std::vector<DepartureResult> &initial_departures = {},
                     std::optional<std::string> initial_station_id = std::nullopt);

    // Destructor - cleanup
    ~DeparturesScreen() override;

    // Get the LVGL screen object
    lv_obj_t *get_screen() const override { return m_screen; }

    // Periodic update - refreshes the clock label
    void update() override;

    // Update the departure table data (call under LVGL lock)
    void update_departures(const std::vector<DepartureResult> &departures);

    // ID of the currently active station tab ("" in split mode or when no stations)
    const std::string &active_station_id() const { return m_active_station_id; }

  private:
    // Forward declaration of helper class
    class DepartureItem;
    struct StationSplitView;

    // Non-copyable
    DeparturesScreen(const DeparturesScreen &) = delete;
    DeparturesScreen &operator=(const DeparturesScreen &) = delete;

    // Helper methods
    void build_header();
    void build_station_tabs();
    void build_split_station_views();
    struct DepartureListWidgets {
        lv_obj_t *list;
        lv_obj_t *spinner;
    };
    DepartureListWidgets build_departure_list(lv_obj_t *parent);
    void build_column_headers(lv_obj_t *parent);
    void apply_station_tab_layout();
    void update_active_station_label();
    void update_clock_label();
    std::vector<DepartureResult> filtered_departures() const;
    std::vector<DepartureResult> filtered_departures(const std::string &station_id) const;
    void render_departures(const std::vector<DepartureResult> &departures);
    void render_departures_for_list(lv_obj_t *list, lv_obj_t *&spinner,
                                    std::unordered_map<std::string, DepartureItem> &departure_items,
                                    const std::vector<DepartureResult> &departures);
    static void station_tab_changed_cb(lv_event_t *e);

    // Member variables
    lv_obj_t *m_screen = nullptr;
    lv_obj_t *m_list = nullptr;
    lv_obj_t *m_station_label = nullptr;
    lv_obj_t *m_station_tabs = nullptr;
    lv_obj_t *m_split_container = nullptr;
    lv_obj_t *m_clock_label = nullptr;
    lv_obj_t *m_spinner = nullptr;
    std::function<void()> m_on_settings_pressed;
    std::vector<StationResult> m_stations;
    bool m_split_mode_enabled = false;
    std::unordered_map<std::string, std::vector<DepartureResult>> m_departures_by_station;
    std::string m_active_station_id;

    // Departure item cache keyed by tripId
    std::unordered_map<std::string, DepartureItem> m_departure_items;
    std::vector<StationSplitView> m_split_views;
};
