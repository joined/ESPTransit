#pragma once

#include "screen_base.h"
#include "app_platform.h"
#include "lvgl.h"
#include <functional>
#include <string>

// WiFi setup screen class - instance-based
class WifiSetupScreen : public ScreenBase {
  public:
    WifiSetupScreen(std::function<void()> on_scan_request,
                    std::function<void(const std::string &ssid, const std::string &password)> on_connect_request,
                    std::function<void()> on_back, bool show_back_button = false);

    // Destructor - cleanup
    ~WifiSetupScreen() override;

    // Get the LVGL screen object
    lv_obj_t *get_screen() const override { return m_screen; }

    // Populate the network list with scan results (call under LVGL lock)
    // current_ssid: optionally pass the currently configured SSID to highlight it
    void populate_networks(const UiWifiNetwork *networks, int count, const char *current_ssid = nullptr);

    // Show connection error (call under LVGL lock)
    void show_connect_error(const std::string &message);

    // Minimal contexts for callbacks
    struct NetworkClickContext {
        std::string ssid;
        bool is_open;
        std::function<void()> clear_highlight;
        std::function<void(const std::string &, const std::string &)> connect;
        std::function<void(const std::string &)> show_password_popup;
    };

    struct PasswordPopupContext {
        std::string ssid;
        lv_obj_t *password_ta;
        std::function<void()> close_popup;
        std::function<void()> clear_highlight;
        std::function<void(const std::string &, const std::string &)> connect;
    };

  private:
    // Non-copyable
    WifiSetupScreen(const WifiSetupScreen &) = delete;
    WifiSetupScreen &operator=(const WifiSetupScreen &) = delete;

    // Static callbacks for LVGL (extract 'this' from user_data)
    static void network_item_cb(lv_event_t *e);
    static void connect_btn_cb(lv_event_t *e);

    // Helper methods
    void show_password_popup(const std::string &ssid);
    void close_password_popup();
    void clear_current_network_highlight();

    // Member variables
    lv_obj_t *m_screen = nullptr;
    lv_obj_t *m_network_list = nullptr;
    lv_obj_t *m_password_backdrop = nullptr;
    lv_obj_t *m_password_popup = nullptr;
    lv_obj_t *m_password_ta = nullptr;
    lv_obj_t *m_current_network_btn = nullptr;
    std::function<void()> m_on_scan_request;
    std::function<void(const std::string &ssid, const std::string &password)> m_on_connect_request;
    std::function<void()> m_on_back;
};
