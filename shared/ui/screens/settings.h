#pragma once

#include "app_constants.h"
#include "screen_base.h"
#include "lvgl.h"
#include "http_client.h"
#include <functional>
#include <cstddef>
#include <string>
#include <vector>

// Settings screen class - instance-based
class SettingsScreen : public ScreenBase {
  public:
    SettingsScreen(std::function<void()> on_back, std::function<void()> on_change_wifi,
                   std::function<void()> on_change_station, std::function<void(uint8_t)> on_rotation_changed,
                   std::function<void(uint8_t)> on_brightness_changed, std::function<void(bool)> on_split_mode_changed,
                   std::function<void(uint8_t)> on_font_size_changed, std::function<bool()> is_wifi_connected,
                   std::function<void()> on_debug, const std::string &ssid, const std::vector<StationResult> &stations,
                   uint8_t rotation, uint8_t brightness, bool split_mode_enabled, uint8_t font_size);

    // Destructor - cleanup
    ~SettingsScreen() override;

    // Get the LVGL screen object
    lv_obj_t *get_screen() const override { return m_screen; }

    // Periodic update - refreshes WiFi connection status
    void update() override;

    // Minimal contexts for callbacks - only what's actually needed
    struct RotationContext {
        uint8_t *current_rotation;
        std::function<void(uint8_t)> on_rotation_changed;
    };

    struct RotationConfirmContext {
        uint8_t old_rotation;
        uint8_t new_rotation;
        lv_obj_t *dropdown; // Needed to revert UI if user cancels (LVGL changes dropdown before our callback)
        RotationContext *rotation_ctx;
    };

    struct BrightnessContext {
        uint8_t *current_brightness;
        std::function<void(uint8_t)> on_brightness_changed;
    };

    struct SplitModeContext {
        bool *current_split_mode;
        std::function<void(bool)> on_split_mode_changed;
    };

    struct FontSizeContext {
        uint8_t *current_font_size;
        std::function<void(uint8_t)> on_font_size_changed;
    };

  private:
    // Non-copyable
    SettingsScreen(const SettingsScreen &) = delete;
    SettingsScreen &operator=(const SettingsScreen &) = delete;

    // Static callbacks for LVGL (extract 'this' from user_data)
    static void rotation_changed_cb(lv_event_t *e);
    static void rotation_confirm_yes_cb(lv_event_t *e);
    static void rotation_confirm_no_cb(lv_event_t *e);
    static void brightness_changed_cb(lv_event_t *e);
    static void split_mode_changed_cb(lv_event_t *e);
    static void font_size_changed_cb(lv_event_t *e);
    static void version_tap_cb(lv_event_t *e);

    // Helper methods
    lv_obj_t *build_card(lv_obj_t *parent);
    void build_wifi_section(lv_obj_t *parent, const std::string &ssid, bool wifi_connected, lv_event_cb_t btn_cb,
                            void *callback_data);
    void build_departure_stations_section(lv_obj_t *parent, lv_event_cb_t btn_cb, void *callback_data);
    void build_dropdown_section(lv_obj_t *parent, const std::string &title, const std::string &options,
                                uint16_t selected, lv_event_cb_t change_cb, void *user_data,
                                const std::string &test_id = {});
    void build_slider_section(lv_obj_t *parent, const std::string &title, int32_t min_value, int32_t max_value,
                              int32_t current_value, const std::string &unit, lv_event_cb_t change_cb, void *user_data);
    void build_switch_section(lv_obj_t *parent, const std::string &title, const std::string &status, bool checked,
                              lv_event_cb_t change_cb, void *user_data);

    // Member variables
    lv_obj_t *m_screen = nullptr;
    std::function<void()> m_on_back;
    std::function<void()> m_on_change_wifi;
    std::function<void()> m_on_change_station;
    std::function<void(uint8_t)> m_on_rotation_changed;
    std::function<void(uint8_t)> m_on_brightness_changed;
    std::function<void(bool)> m_on_split_mode_changed;
    std::function<void(uint8_t)> m_on_font_size_changed;
    std::function<bool()> m_is_wifi_connected;
    uint8_t m_current_rotation = APP_CONFIG_DEFAULT_ROTATION;
    uint8_t m_current_brightness = APP_CONFIG_DEFAULT_BRIGHTNESS;
    bool m_current_split_mode = APP_CONFIG_DEFAULT_SPLIT_MODE;
    uint8_t m_current_font_size = 0;
    lv_obj_t *m_wifi_connected_label = nullptr;
    RotationContext m_rotation_context;
    BrightnessContext m_brightness_context;
    SplitModeContext m_split_mode_context;
    FontSizeContext m_font_size_context;
    bool m_wifi_connected = false;
    std::function<void()> m_on_debug;
    std::string m_ssid;
    uint32_t m_version_tap_count = 0;
    uint32_t m_last_tap_time = 0;
    std::vector<StationResult> m_stations;
    bool m_split_mode_available = false;
};
