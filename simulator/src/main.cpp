#include "config_check.h"
#include "FreeRTOS.h"
#include "task.h"

#include <iostream>
#include <vector>

#include "esp_log.h"
#include "app_manager.h"
#include "app_state.h"
#include "bsp/display.h"
#include "sim_cli.h"
#include "sim_control.h"

static const char *TAG = "main";

static AppManager s_app_manager;

// Global flags (accessed by other modules)
bool g_mock_http = false;
int g_initial_rotation = -1; // -1 = use saved config, 0/90/180/270 = override

int main(int argc, char **argv)
{
    CliOptions options;
    if (!parse_args(argc, argv, options)) {
        return 1;
    }
    if (options.show_help) {
        return 0;
    }

    // Load board definitions from boards.json.
    std::vector<BoardInfo> boards;
    if (!load_boards(boards)) {
        return 1;
    }

    // Resolve board: explicit --board flag or default (first entry).
    const BoardInfo *board = nullptr;
    if (options.board.has_value()) {
        board = find_board(boards, *options.board);
        if (board == nullptr) {
            std::cerr << "Unknown board: " << *options.board << "\n";
            std::cerr << "Available boards: " << available_board_names(boards) << "\n";
            return 1;
        }
    } else {
        board = &boards[0];
    }

    if (!sim_set_base_resolution(board->width, board->height)) {
        std::cerr << "Failed to set display resolution for board " << board->name << "\n";
        return 1;
    }
    std::cout << "Board: " << board->name << " (" << board->width << "x" << board->height << ")\n";

    // Apply runtime config path before any config operations.
    config_set_file_path(options.config_file.c_str());

    if (options.zoom.has_value()) {
        sim_set_initial_zoom(*options.zoom);
    }
    if (options.rotation.has_value()) {
        g_initial_rotation = *options.rotation;
        std::cout << "Display rotation set to " << g_initial_rotation << " degrees\n";
    }
    if (options.mock_http) {
        g_mock_http = true;
        std::cout << "Mock HTTP mode enabled - no network requests will be made\n";
    }

    // Apply CLI overrides to config (if any)
    if (g_initial_rotation >= 0) {
        AppConfig config = {};
        config_load(&config);

        // Convert degrees to rotation index (0°->0, 90°->1, 180°->2, 270°->3)
        uint8_t rotation_index = g_initial_rotation / 90;
        ESP_LOGI(TAG, "Overriding rotation: %d° -> %d°", config.rotation * 90, g_initial_rotation);
        config.rotation = rotation_index;

        config_save(config);
    }

    // Start the application manager (creates queues and tasks)
    s_app_manager.start();
    if (options.control_http) {
        if (!sim_control_start_http_task("127.0.0.1", options.control_http_port)) {
            std::cerr << "Failed to create HTTP control task\n";
            return 1;
        }
    }

    vTaskStartScheduler();

    return 0;
}
