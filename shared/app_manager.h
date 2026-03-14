#pragma once

#include "app_state.h"
#include "app_platform.h"
#include "http_fetcher.h"
#include "ui/screens/screen_base.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "lvgl.h"
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>

// Consolidated global state for the application
struct AppGlobalState {
    AppConfig config;
    AppState state = AppState::BOOT;
    AppState previous_state = AppState::BOOT;
    TaskHandle_t app_task_handle = nullptr;
    bool last_wifi_attempt_failed = false;
};

// Timer configuration for state-managed timers
struct TimerConfig {
    std::function<void()> callback; // Called when timer fires
    uint32_t interval_ms;           // Timer period in milliseconds
};

// Configuration for each app state's lifecycle
struct StateConfig {
    std::function<std::unique_ptr<ScreenBase>()> init; // Constructs screen object (stores callbacks/params)
    std::function<void()> on_enter;                    // Called after screen is loaded
    std::function<void()> on_exit;                     // Called before screen is destroyed
    std::vector<TimerConfig> timers;                   // Timers active while in this state
    uint32_t screen_update_interval_ms = 0;            // Auto-creates locked_update() timer if > 0
};

// Command type for the command queue
using Command = std::function<void()>;

class BootScreen;
class DeparturesScreen;
class StationSearchScreen;
class WifiSetupScreen;

class AppManager {
  public:
    AppManager();

    // Initialize queues and start tasks
    void start();

    // Generic state transition
    void transitionTo(AppState new_state);

    // Queue a command to run on the main task
    void postCommand(Command cmd);

    // Task loop
    void runMainLoop();

    // Static task entry point for FreeRTOS
    static void mainTaskEntry(void *arg);

  private:
    AppGlobalState state_;
    HttpFetcher http_fetcher_;
    QueueHandle_t command_queue_ = nullptr;
    std::unordered_map<AppState, StateConfig> state_configs_;
    struct ActiveTimer {
        TimerHandle_t handle;
        std::function<void()> callback;
    };
    std::vector<ActiveTimer> active_timers_;
    uint32_t timer_generation_ = 0;

    // Current screen instance
    std::unique_ptr<ScreenBase> m_current_screen;

    // Brightness save debounce
    TimerHandle_t brightness_debounce_timer_ = nullptr;
    uint8_t pending_brightness_ = 0;

    // Search generation counter for discarding stale results
    uint32_t search_generation_ = 0;
    std::optional<uint32_t> in_flight_search_generation_;
    std::optional<std::string> pending_search_query_;

    // Departures screen UI state cached across screen transitions.
    std::vector<DepartureResult> cached_departures_;
    std::optional<std::string> last_departures_station_id_;

    void initStateConfigs();
    void stopAllTimers();
    void startTimersForState(const StateConfig &config);
    static void timerCallbackEntry(TimerHandle_t timer);

    // Wraps a void() handler to post it to the main task queue
    std::function<void()> posted(std::function<void()> fn);
    bool enqueueStationSearchRequest(const std::string &query, uint32_t generation);
    void saveBrightnessNow();
    DeparturesScreen *currentDeparturesScreen() const;
    StationSearchScreen *currentStationSearchScreen() const;
    WifiSetupScreen *currentWifiSetupScreen() const;

    // Command handlers
    void onWifiScan();
    void onWifiConnect(const std::string &ssid, const std::string &password);
    void onWifiBack();
    void onStationSearch(const std::string &query);
    void onStationSearchCancel();
    void onStationsConfigured(const std::vector<StationResult> &stations);
    void onDeparturesRefresh();
    void onRotationChanged(uint8_t rotation);
    void onBrightnessChanged(uint8_t brightness);
    void onSplitModeChanged(bool split_mode_enabled);
    void onFontSizeChanged(uint8_t font_size);

    std::vector<StationResult> getConfiguredStations() const;
    void saveConfiguredStations(const std::vector<StationResult> &stations);

    // HTTP result handlers (run on main task via postCommand)
    void onDeparturesResult(esp_err_t error, std::shared_ptr<std::vector<DepartureResult>> departures);
    void onStationsResult(esp_err_t error, std::shared_ptr<std::vector<StationResult>> stations, uint32_t generation);

    // WiFi connection helper (shared between onWifiConnect and boot_init)
    static bool wifiConnectWithSpinner(std::function<bool()> connect_fn);

    // Platform-agnostic boot_init_late implementation (display + WiFi init)
    static PlatformBootResult bootInitLate(const AppConfig &config, BootScreen *boot_screen);
};
