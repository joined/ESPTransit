#include "app_manager.h"
#include "app_constants.h"
#include "app_platform.h"
#include "http_client.h"
#include "station_config_utils.h"
#include "ui/common.h"
#include "ui/screens/wifi_setup.h"
#include "ui/screens/station_search.h"
#include "ui/screens/departures.h"
#include "ui/screens/settings.h"
#include "ui/screens/boot.h"
#include "ui/screens/debug.h"
#include "esp_log.h"
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "wifi_manager.h"
#include <cstdio>

static const char *TAG = "app_manager";

static FontSize board_default_font_size()
{
    lv_display_t *disp = lv_display_get_default();
    if (!disp) {
        ESP_LOGW(TAG, "No default display, using SMALL font preset fallback");
        return FontSize::SMALL;
    }

    const int32_t h_res = lv_display_get_horizontal_resolution(disp);
    const int32_t v_res = lv_display_get_vertical_resolution(disp);
    const int32_t short_edge = (h_res < v_res) ? h_res : v_res;
    if (short_edge <= 480)
        return FontSize::LARGE;
    if (short_edge <= 600)
        return FontSize::MEDIUM;
    return FontSize::SMALL;
}

static FontSize resolve_effective_font_size(uint8_t configured_font_size)
{
    if (configured_font_size <= 2) {
        return static_cast<FontSize>(configured_font_size);
    }
    return board_default_font_size();
}

// Per-timer context stored as the FreeRTOS timer ID. Heap-allocated so the
// pointer remains stable regardless of vector resizing or SSO internals.
struct TimerContext {
    AppManager *manager;
    uint32_t generation;
    size_t index;
};

static const char *appStateName(AppState state)
{
    switch (state) {
    case AppState::BOOT:
        return "BOOT";
    case AppState::WIFI_SETUP:
        return "WIFI_SETUP";
    case AppState::WIFI_CONNECTING:
        return "WIFI_CONNECTING";
    case AppState::STATION_SEARCH:
        return "STATION_SEARCH";
    case AppState::DEPARTURES:
        return "DEPARTURES";
    case AppState::SETTINGS:
        return "SETTINGS";
    case AppState::DEBUG:
        return "DEBUG";
    }
    return "UNKNOWN";
}

AppManager::AppManager()
    : http_fetcher_([this](auto cmd) { postCommand(std::move(cmd)); },
                    [this](esp_err_t error, std::shared_ptr<std::vector<DepartureResult>> departures) {
                        onDeparturesResult(error, departures);
                    },
                    [this](esp_err_t error, std::shared_ptr<std::vector<StationResult>> stations, uint32_t gen) {
                        onStationsResult(error, stations, gen);
                    })
{
    initStateConfigs();
}

void AppManager::initStateConfigs()
{
    state_configs_ = {
        {AppState::WIFI_SETUP,
         {
             .init = [this]() -> std::unique_ptr<ScreenBase> {
                 bool show_back = state_.previous_state == AppState::SETTINGS;
                 return std::make_unique<WifiSetupScreen>(
                     posted([this] { onWifiScan(); }),
                     [this](const std::string &ssid, const std::string &password) {
                         postCommand([this, ssid, password] { onWifiConnect(ssid, password); });
                     },
                     posted([this] { onWifiBack(); }), show_back);
             },
             .on_enter = posted([this] { onWifiScan(); }),
             .on_exit = nullptr,
             .timers = {},
         }},
        {AppState::STATION_SEARCH,
         {
             .init = [this]() -> std::unique_ptr<ScreenBase> {
                 bool show_back = state_.previous_state == AppState::SETTINGS;
                 std::vector<StationResult> initial_selection = getConfiguredStations();
                 return std::make_unique<StationSearchScreen>(
                     [this](const std::string &query) { postCommand([this, query] { onStationSearch(query); }); },
                     posted([this] { onStationSearchCancel(); }),
                     [this](const std::vector<StationResult> &stations) {
                         postCommand([this, stations] { onStationsConfigured(stations); });
                     },
                     posted([this] { transitionTo(AppState::SETTINGS); }), initial_selection, show_back);
             },
             .on_enter = nullptr,
             .on_exit =
                 [this] {
                     // Invalidate in-flight searches and clear any coalesced query.
                     ++search_generation_;
                     pending_search_query_.reset();
                 },
             .timers = {},
         }},
        {AppState::DEPARTURES,
         {
             .init = [this]() -> std::unique_ptr<ScreenBase> {
                 std::vector<StationResult> stations = getConfiguredStations();
                 return std::make_unique<DeparturesScreen>(posted([this] { transitionTo(AppState::SETTINGS); }),
                                                           stations, state_.config.split_mode, cached_departures_,
                                                           last_departures_station_id_);
             },
             .on_enter = posted([this] { onDeparturesRefresh(); }),
             .on_exit =
                 [this] {
                     auto *departures = currentDeparturesScreen();
                     if (departures && !departures->active_station_id().empty()) {
                         last_departures_station_id_ = departures->active_station_id();
                     }
                 },
             .timers = {{
                 .callback = posted([this] { onDeparturesRefresh(); }),
                 .interval_ms = DEPARTURES_REFRESH_INTERVAL_MS,
             }},
             .screen_update_interval_ms = SCREEN_CLOCK_UPDATE_INTERVAL_MS,
         }},
        {AppState::SETTINGS,
         {
             .init = [this]() -> std::unique_ptr<ScreenBase> {
                 std::vector<StationResult> stations = getConfiguredStations();
                 return std::make_unique<SettingsScreen>(
                     posted([this] {
                         transitionTo(getConfiguredStations().empty() ? AppState::STATION_SEARCH
                                                                      : AppState::DEPARTURES);
                     }),
                     posted([this] { transitionTo(AppState::WIFI_SETUP); }),
                     posted([this] { transitionTo(AppState::STATION_SEARCH); }),
                     [this](uint8_t rotation) { postCommand([this, rotation] { onRotationChanged(rotation); }); },
                     [this](uint8_t brightness) {
                         postCommand([this, brightness] { onBrightnessChanged(brightness); });
                     },
                     [this](bool split_mode_enabled) {
                         postCommand([this, split_mode_enabled] { onSplitModeChanged(split_mode_enabled); });
                     },
                     [this](uint8_t font_size) { postCommand([this, font_size] { onFontSizeChanged(font_size); }); },
                     [] { return AppPlatform::is_wifi_connected(); }, posted([this] { transitionTo(AppState::DEBUG); }),
                     AppPlatform::get_current_ssid(), stations, state_.config.rotation, state_.config.brightness,
                     state_.config.split_mode, static_cast<uint8_t>(ui_get_font_size()));
             },
             .on_enter = nullptr,
             .on_exit = nullptr,
             .timers = {},
             .screen_update_interval_ms = 2000,
         }},
        {AppState::DEBUG,
         {
             .init = [this]() -> std::unique_ptr<ScreenBase> {
                 return std::make_unique<DebugScreen>(posted([this] { transitionTo(AppState::SETTINGS); }));
             },
             .on_enter = nullptr,
             .on_exit = nullptr,
             .timers = {},
             .screen_update_interval_ms = 1000,
         }},
    };
}

void AppManager::postCommand(Command cmd)
{
    auto *fn = new Command(std::move(cmd));
    if (xQueueSend(command_queue_, &fn, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Command queue full, dropping command");
        delete fn;
    }
}

void AppManager::start()
{
    // Create command queue for main task (holds pointers to Command lambdas)
    command_queue_ = xQueueCreate(APP_COMMAND_QUEUE_LENGTH, sizeof(Command *));
    if (!command_queue_) {
        ESP_LOGE(TAG, "Failed to create command queue");
        abort();
    }

    // Start HTTP fetcher task
    http_fetcher_.start();

    // Start application task
    xTaskCreate(AppManager::mainTaskEntry, "app_task", 8192, this, 5, &state_.app_task_handle);
}

std::function<void()> AppManager::posted(std::function<void()> fn)
{
    return [this, fn = std::move(fn)]() { postCommand(fn); };
}

void AppManager::stopAllTimers()
{
    ++timer_generation_;
    for (auto &t : active_timers_) {
        delete static_cast<TimerContext *>(pvTimerGetTimerID(t.handle));
        xTimerStop(t.handle, portMAX_DELAY);
        xTimerDelete(t.handle, portMAX_DELAY);
    }
    active_timers_.clear();
}

void AppManager::startTimersForState(const StateConfig &config)
{
    // Build combined callback list: explicit timers + screen_update
    std::vector<TimerConfig> all_timers = config.timers;
    if (config.screen_update_interval_ms > 0) {
        all_timers.push_back({
            .callback = [this]() { m_current_screen->locked_update(); },
            .interval_ms = config.screen_update_interval_ms,
        });
    }

    uint32_t gen = timer_generation_;
    for (size_t i = 0; i < all_timers.size(); ++i) {
        auto *ctx = new TimerContext{this, gen, i};
        TimerHandle_t timer =
            xTimerCreate("t", pdMS_TO_TICKS(all_timers[i].interval_ms), pdTRUE, ctx, AppManager::timerCallbackEntry);
        if (timer) {
            xTimerStart(timer, portMAX_DELAY);
            active_timers_.push_back({timer, all_timers[i].callback});
            ESP_LOGI(TAG, "Started timer %zu/%lu (%lu ms)", i, (unsigned long)gen,
                     (unsigned long)all_timers[i].interval_ms);
        } else {
            delete ctx;
            ESP_LOGE(TAG, "Failed to create timer %zu/%lu", i, (unsigned long)gen);
        }
    }
}

void AppManager::timerCallbackEntry(TimerHandle_t timer)
{
    auto *ctx = static_cast<TimerContext *>(pvTimerGetTimerID(timer));
    if (!ctx) {
        return;
    }

    AppManager *manager = ctx->manager;
    manager->postCommand([manager, gen = ctx->generation, idx = ctx->index] {
        if (gen != manager->timer_generation_) {
            return; // stale: fired just before a state transition
        }
        if (idx < manager->active_timers_.size()) {
            manager->active_timers_[idx].callback();
        }
    });
}

bool AppManager::enqueueStationSearchRequest(const std::string &query, uint32_t generation)
{
    auto *station_search = currentStationSearchScreen();
    if (!station_search) {
        return false;
    }

    bsp_display_lock(0);
    station_search->show_loading();
    bsp_display_unlock();

    HttpRequest request;
    request.type = HTTP_REQUEST_STATION_SEARCH;
    request.generation = generation;
    snprintf(request.search_query, sizeof(request.search_query), "%s", query.c_str());

    if (!http_fetcher_.enqueue(request)) {
        bsp_display_lock(0);
        station_search->hide_loading();
        bsp_display_unlock();
        ESP_LOGW(TAG, "Station search request dropped (queue full)");
        return false;
    }

    in_flight_search_generation_ = generation;
    return true;
}

std::vector<StationResult> AppManager::getConfiguredStations() const
{
    return normalize_station_list(state_.config.stations);
}

void AppManager::saveConfiguredStations(const std::vector<StationResult> &stations)
{
    state_.config.stations = normalize_station_list(stations);
    state_.config.configured = state_.config.configured || !state_.config.stations.empty();
    config_save(state_.config);
}

DeparturesScreen *AppManager::currentDeparturesScreen() const
{
    if (state_.state != AppState::DEPARTURES || !m_current_screen) {
        return nullptr;
    }
    return static_cast<DeparturesScreen *>(m_current_screen.get());
}

StationSearchScreen *AppManager::currentStationSearchScreen() const
{
    if (state_.state != AppState::STATION_SEARCH || !m_current_screen) {
        return nullptr;
    }
    return static_cast<StationSearchScreen *>(m_current_screen.get());
}

WifiSetupScreen *AppManager::currentWifiSetupScreen() const
{
    if (state_.state != AppState::WIFI_SETUP || !m_current_screen) {
        return nullptr;
    }
    return static_cast<WifiSetupScreen *>(m_current_screen.get());
}

void AppManager::transitionTo(AppState new_state)
{
    ESP_LOGI(TAG, "Transitioning from %s to %s", appStateName(state_.state), appStateName(new_state));

    auto new_it = state_configs_.find(new_state);
    if (new_it == state_configs_.end()) {
        ESP_LOGE(TAG, "No config for state %s", appStateName(new_state));
        return;
    }

    // Stop all timers before destroying the screen
    stopAllTimers();

    bsp_display_lock(0);

    // Exit current state
    auto old_it = state_configs_.find(state_.state);
    if (old_it != state_configs_.end()) {
        if (old_it->second.on_exit) {
            old_it->second.on_exit();
        }
    }

    // Update state
    state_.previous_state = state_.state;
    state_.state = new_state;

    // Create and load the new screen before destroying the old one
    // to avoid deleting the active screen (LVGL warning)
    std::unique_ptr<ScreenBase> old_screen = std::move(m_current_screen);
    if (new_it->second.init) {
        m_current_screen = new_it->second.init();
        lv_screen_load(m_current_screen->get_screen());
    }
    old_screen.reset();

    bsp_display_unlock();

    // Start new timers after creating the screen
    startTimersForState(new_it->second);

    // Post-transition callback (outside lock for operations like task notifications)
    if (new_it->second.on_enter) {
        new_it->second.on_enter();
    }
}

// WiFi connection helper

bool AppManager::wifiConnectWithSpinner(std::function<bool()> connect_fn)
{
    bsp_display_lock(0);
    lv_obj_t *spinner = ui_spinner_show("Connecting...");
    bsp_display_unlock();

    bool success = connect_fn();

    if (success) {
        bsp_display_lock(0);
        lv_label_set_text(lv_obj_get_child(spinner, 1), "Syncing time...");
        bsp_display_unlock();

        AppPlatform::sntp_sync();

        bsp_display_lock(0);
        ui_spinner_hide(spinner);
        bsp_display_unlock();
    } else {
        bsp_display_lock(0);
        ui_spinner_hide(spinner);
        bsp_display_unlock();
    }

    return success;
}

PlatformBootResult AppManager::bootInitLate(const AppConfig &config, BootScreen *boot_screen)
{
    // Apply display rotation
    bsp_display_lock(0);
    lv_display_t *disp = lv_display_get_default();
    bsp_display_rotate(disp, static_cast<lv_disp_rotation_t>(config.rotation));

    // Log the actual display dimensions after rotation
    int32_t h_res = lv_display_get_horizontal_resolution(disp);
    int32_t v_res = lv_display_get_vertical_resolution(disp);
    ESP_LOGI(TAG, "Display after rotation: %ldx%ld", h_res, v_res);

    // Load the boot screen now that rotation is applied
    lv_screen_load(boot_screen->get_screen());
    bsp_display_unlock();

    // Turn on backlight after rotation is applied and boot screen is loaded
    uint8_t brightness = config.brightness > 0 ? config.brightness : APP_CONFIG_DEFAULT_BRIGHTNESS;
    bsp_display_brightness_set(brightness);
    ESP_LOGI(TAG, "Display brightness set to %d%%", brightness);

    // Initialize WiFi
    PlatformBootResult result = {
        .success = false,
        .should_show_wifi_setup = false,
    };

    if (config.configured) {
        // Auto-connect: init WiFi with auto_connect=true so credentials are used
        ESP_LOGI(TAG, "Config found, auto-connecting...");

        bsp_display_lock(0);
        boot_screen->set_status("Connecting...");
        bsp_display_unlock();

        ESP_ERROR_CHECK(wifi_manager_init(true));

        bool success = wifi_manager_wait_connected(WIFI_CONNECT_TIMEOUT_MS) == ESP_OK;

        if (success) {
            bsp_display_lock(0);
            boot_screen->set_status("Syncing time...");
            bsp_display_unlock();

            AppPlatform::sntp_sync();

            result.success = true;
            result.should_show_wifi_setup = false;
        } else {
            ESP_LOGW(TAG, "Auto-connect failed, showing WiFi setup");

            bsp_display_lock(0);
            boot_screen->set_status("Connection failed");
            bsp_display_unlock();
            vTaskDelay(pdMS_TO_TICKS(BOOT_CONNECT_FAILED_DELAY_MS));

            result.success = false;
            result.should_show_wifi_setup = true;
        }
    } else {
        // No config: init WiFi without auto-connect, show setup wizard
        ESP_LOGI(TAG, "No config found, starting WiFi setup");
        ESP_ERROR_CHECK(wifi_manager_init(false));
        // Brief pause so boot screen is visible before WiFi setup
        vTaskDelay(pdMS_TO_TICKS(BOOT_NO_CONFIG_DELAY_MS));
        result.success = false;
        result.should_show_wifi_setup = true;
    }

    return result;
}

// Notification handlers

void AppManager::onWifiScan()
{
    ESP_LOGI(TAG, "Handling WiFi scan request");

    bsp_display_lock(0);
    lv_obj_t *spinner = ui_spinner_show("Scanning...");
    bsp_display_unlock();

    // Platform does the actual scan
    UiWifiNetwork results[WIFI_SCAN_MAX_RESULTS];
    const char *current_ssid = nullptr;
    int count = AppPlatform::wifi_do_scan(results, WIFI_SCAN_MAX_RESULTS, &current_ssid);

    bsp_display_lock(0);
    ui_spinner_hide(spinner);
    auto *wifi = currentWifiSetupScreen();
    if (!wifi) {
        bsp_display_unlock();
        return;
    }
    if (count >= 0) {
        wifi->populate_networks(results, count, current_ssid);
    } else {
        wifi->show_connect_error("WiFi scan failed. Please try again.");
    }
    bsp_display_unlock();
}

void AppManager::onWifiConnect(const std::string &ssid, const std::string &password)
{
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid.c_str());

    bool success =
        wifiConnectWithSpinner([&]() { return wifi_manager_connect(ssid.c_str(), password.c_str()) == ESP_OK; });

    if (success) {
        if (!state_.config.configured) {
            state_.config.configured = true;
            config_save(state_.config);
        }
        state_.last_wifi_attempt_failed = false;
        bool has_configured_stations = !getConfiguredStations().empty();

        // Go back to where we came from, or continue initial setup flow
        if (state_.previous_state == AppState::SETTINGS) {
            transitionTo(AppState::SETTINGS);
        } else if (state_.config.configured && has_configured_stations) {
            transitionTo(AppState::DEPARTURES);
        } else {
            transitionTo(AppState::STATION_SEARCH);
        }
    } else {
        state_.last_wifi_attempt_failed = true;
        bsp_display_lock(0);
        auto *wifi = currentWifiSetupScreen();
        if (wifi) {
            wifi->show_connect_error("Could not connect. Check password and try again.");
        }
        bsp_display_unlock();
    }
}

void AppManager::onWifiBack()
{
    ESP_LOGI(TAG, "WiFi back pressed");

    // Try to reconnect to previous network (unless last attempt just failed)
    if (!state_.last_wifi_attempt_failed) {
        bsp_display_lock(0);
        lv_obj_t *spinner = ui_spinner_show("Reconnecting...");
        bsp_display_unlock();

        bool reconnected = AppPlatform::wifi_do_reconnect();

        bsp_display_lock(0);
        ui_spinner_hide(spinner);
        bsp_display_unlock();

        if (reconnected) {
            ESP_LOGI(TAG, "Reconnected to previous WiFi network");
        }
    }

    // Reset the flag when going back to settings
    state_.last_wifi_attempt_failed = false;
    transitionTo(AppState::SETTINGS);
}

void AppManager::onStationSearch(const std::string &query)
{
    ESP_LOGI(TAG, "Starting search for: %s", query.c_str());

    ++search_generation_;

    if (in_flight_search_generation_.has_value()) {
        // Keep only the latest query while a request is in flight.
        pending_search_query_ = query;
        ESP_LOGI(TAG, "Coalescing search request (generation %lu)", (unsigned long)search_generation_);
        return;
    }

    enqueueStationSearchRequest(query, search_generation_);
}

void AppManager::onStationSearchCancel()
{
    ++search_generation_;
    pending_search_query_.reset();
}

void AppManager::onStationsConfigured(const std::vector<StationResult> &stations)
{
    ESP_LOGI(TAG, "Configured %d station(s)", (int)stations.size());

    std::vector<StationResult> previous_stations = getConfiguredStations();
    saveConfiguredStations(stations);

    if (!station_lists_equal(previous_stations, getConfiguredStations())) {
        // Clear cached departures when station configuration changes.
        cached_departures_.clear();
        last_departures_station_id_ = std::nullopt;
    }

    transitionTo(AppState::DEPARTURES);
}

void AppManager::onDeparturesRefresh()
{
    ESP_LOGI(TAG, "Refreshing departures");

    std::vector<StationResult> stations = getConfiguredStations();
    if (stations.empty()) {
        ESP_LOGW(TAG, "No configured station IDs, skipping departures refresh");
        return;
    }

    HttpRequest request;
    request.type = HTTP_REQUEST_DEPARTURES;
    const std::string station_ids = build_station_ids_csv(stations);
    snprintf(request.station_ids, sizeof(request.station_ids), "%s", station_ids.c_str());
    if (!http_fetcher_.enqueue(request)) {
        ESP_LOGW(TAG, "Departures refresh skipped (HTTP queue full)");
    }
}

void AppManager::onRotationChanged(uint8_t rotation)
{
    ESP_LOGI(TAG, "Rotation changed to %d", rotation);
    uint8_t old_rotation = state_.config.rotation;
    state_.config.rotation = rotation;
    config_save(state_.config);
    AppPlatform::handle_rotation_change(state_.config, old_rotation);
}

void AppManager::onBrightnessChanged(uint8_t brightness)
{
    ESP_LOGI(TAG, "Brightness changed to %d", brightness);

    // Apply brightness immediately for responsive feedback
    state_.config.brightness = brightness;
    AppPlatform::handle_brightness_change(state_.config);

    // Debounce the config save to avoid flash wear
    pending_brightness_ = brightness;

    if (brightness_debounce_timer_) {
        // Reset the timer if it's already running
        xTimerReset(brightness_debounce_timer_, 0);
    } else {
        // Create a one-shot timer to save after 1 second of no changes
        brightness_debounce_timer_ = xTimerCreate("brightness_save", pdMS_TO_TICKS(1000),
                                                  pdFALSE, // One-shot
                                                  this, [](TimerHandle_t timer) {
                                                      AppManager *mgr =
                                                          static_cast<AppManager *>(pvTimerGetTimerID(timer));
                                                      mgr->saveBrightnessNow();
                                                  });
        xTimerStart(brightness_debounce_timer_, 0);
    }
}

void AppManager::onSplitModeChanged(bool split_mode_enabled)
{
    ESP_LOGI(TAG, "Split mode changed to %d", split_mode_enabled ? 1 : 0);
    state_.config.split_mode = split_mode_enabled;
    config_save(state_.config);
}

void AppManager::onFontSizeChanged(uint8_t font_size)
{
    ESP_LOGI(TAG, "Font size changed to %d", font_size);
    state_.config.font_size = font_size;
    config_save(state_.config);
    ui_set_font_size(static_cast<FontSize>(font_size));

    // Re-transition to SETTINGS to rebuild the screen with new font sizes
    transitionTo(AppState::SETTINGS);
}

void AppManager::saveBrightnessNow()
{
    ESP_LOGI(TAG, "Saving brightness config: %d", pending_brightness_);
    state_.config.brightness = pending_brightness_;
    config_save(state_.config);
}

// Main application task

void AppManager::runMainLoop()
{
    // Platform-specific boot initialization
    AppPlatform::boot_init_early(state_.config);

    ui_set_font_size(resolve_effective_font_size(state_.config.font_size));

    // Create boot screen — shown during WiFi connect and time sync
    auto boot_screen = std::make_unique<BootScreen>();
    auto boot_result = bootInitLate(state_.config, boot_screen.get());

    // transitionTo will load the new screen, making boot screen no longer active
    if (boot_result.should_show_wifi_setup) {
        transitionTo(AppState::WIFI_SETUP);
    } else if (getConfiguredStations().empty()) {
        transitionTo(AppState::STATION_SEARCH);
    } else {
        transitionTo(AppState::DEPARTURES);
    }
    // Destroy boot screen after transition loaded the new screen
    boot_screen.reset();

    // Main event loop - process commands from queue
    Command *cmd = nullptr;
    while (true) {
        if (xQueueReceive(command_queue_, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
            (*cmd)();
            delete cmd;
        }
    }
}

// HTTP result handlers (called on main task via postCommand)

void AppManager::onDeparturesResult(esp_err_t error, std::shared_ptr<std::vector<DepartureResult>> departures)
{
    auto *departures_screen = currentDeparturesScreen();
    if (!departures_screen) {
        ESP_LOGI(TAG, "Discarding departures result (no longer on departures screen)");
        return;
    }
    if (error == ESP_OK) {
        cached_departures_ = *departures;
        bsp_display_lock(0);
        departures_screen->update_departures(*departures);
        bsp_display_unlock();
        ESP_LOGI(TAG, "Departures updated successfully");
    } else {
        ESP_LOGW(TAG, "Failed to fetch departures");
    }
}

void AppManager::onStationsResult(esp_err_t error, std::shared_ptr<std::vector<StationResult>> stations,
                                  uint32_t generation)
{
    if (in_flight_search_generation_.has_value() && *in_flight_search_generation_ == generation) {
        in_flight_search_generation_.reset();
    }

    auto dispatch_pending_search = [this]() {
        if (state_.state != AppState::STATION_SEARCH || !pending_search_query_.has_value()) {
            return;
        }

        if (in_flight_search_generation_.has_value()) {
            return;
        }

        if (enqueueStationSearchRequest(*pending_search_query_, search_generation_)) {
            pending_search_query_.reset();
        }
    };

    // Discard results if we've left the station search screen
    auto *station_search = currentStationSearchScreen();
    if (!station_search) {
        ESP_LOGI(TAG, "Discarding search results (no longer on station search screen)");
        pending_search_query_.reset();
        return;
    }
    // Discard stale results if user has triggered a newer search
    if (generation != search_generation_) {
        ESP_LOGI(TAG, "Discarding stale search results (gen %lu vs %lu)", (unsigned long)generation,
                 (unsigned long)search_generation_);
        dispatch_pending_search();
        return;
    }

    bsp_display_lock(0);
    station_search->hide_loading();
    bsp_display_unlock();

    if (error != ESP_OK) {
        bsp_display_lock(0);
        station_search->show_error("Search failed. Check your connection.");
        bsp_display_unlock();
        ESP_LOGW(TAG, "Station search failed");
        dispatch_pending_search();
        return;
    }

    if (stations->empty()) {
        bsp_display_lock(0);
        station_search->begin_update(*stations);
        station_search->finish_update();
        station_search->show_error("No stations found.");
        bsp_display_unlock();
        dispatch_pending_search();
        return;
    }

    // Diff-based update: remove stale items and find which stations need creation
    bsp_display_lock(0);
    auto to_create = station_search->begin_update(*stations);
    bsp_display_unlock();

    // Create new items one at a time, yielding between each to let input through
    for (const auto &sr : *stations) {
        if (to_create.find(sr.id) == to_create.end())
            continue;
        if (generation != search_generation_) {
            ESP_LOGI(TAG, "Aborting stale result population");
            break;
        }
        bsp_display_lock(0);
        station_search->create_result_item(sr);
        bsp_display_unlock();
        vTaskDelay(1); // Yield to let LVGL process input
    }

    // Reorder items to match API result order
    if (generation == search_generation_) {
        bsp_display_lock(0);
        station_search->finish_update();
        bsp_display_unlock();
    }

    ESP_LOGI(TAG, "Station search completed: %d results, %d new", (int)stations->size(), (int)to_create.size());
    dispatch_pending_search();
}

// Static task entry point for FreeRTOS

void AppManager::mainTaskEntry(void *arg)
{
    static_cast<AppManager *>(arg)->runMainLoop();
}
