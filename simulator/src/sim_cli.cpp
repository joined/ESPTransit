#include "sim_cli.h"

#include <iostream>

#include <ArduinoJson.h>
#include <argparse/argparse.hpp>

#include "app_state.h"

extern const char *const kBoardsJsonContent;

static bool is_allowed_rotation(int rotation)
{
    return rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270;
}

bool load_boards(std::vector<BoardInfo> &boards)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, kBoardsJsonContent);
    if (error) {
        std::cerr << "Failed to parse boards.json: " << error.c_str() << "\n";
        return false;
    }

    JsonObject root = doc.as<JsonObject>();
    for (JsonPair kv : root) {
        JsonObject board = kv.value().as<JsonObject>();
        boards.push_back({std::string(kv.key().c_str()), board["width"], board["height"]});
    }

    return !boards.empty();
}

std::string available_board_names(const std::vector<BoardInfo> &boards)
{
    std::string result;
    for (size_t i = 0; i < boards.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += boards[i].name;
    }
    return result;
}

const BoardInfo *find_board(const std::vector<BoardInfo> &boards, const std::string &name)
{
    for (const auto &b : boards) {
        if (b.name == name) {
            return &b;
        }
    }
    return nullptr;
}

bool parse_args(int argc, char **argv, CliOptions &options)
{
    argparse::ArgumentParser parser("esptransit_sim", "1.0", argparse::default_arguments::all, false);

    parser.add_description("ESPTransit desktop simulator");
    parser.add_argument("-z", "--zoom").help("Set window zoom level (e.g., 0.8 for 80%)").scan<'g', float>();
    parser.add_argument("-m", "--mock")
        .help("Enable mock HTTP responses (for offline development)")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--control-http")
        .help("Enable live HTTP automation server on localhost")
        .default_value(false)
        .implicit_value(true);
    parser.add_argument("--control-http-port")
        .help("Control HTTP server port (required with --control-http)")
        .default_value(0)
        .scan<'i', int>();
    parser.add_argument("-r", "--rotation").help("Set display rotation (0, 90, 180, or 270)").scan<'i', int>();
    parser.add_argument("-b", "--board").help("Target board (determines display resolution)").nargs(1);
    parser.add_argument("--config-file")
        .help("Use custom simulator config file path")
        .default_value(std::string(kDefaultSimConfigFilePath))
        .nargs(1);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << "\n";
        std::cerr << parser;
        return false;
    }

    options.mock_http = parser.get<bool>("--mock");
    options.control_http = parser.get<bool>("--control-http");
    options.control_http_port = parser.get<int>("--control-http-port");
    options.config_file = parser.get<std::string>("--config-file");
    options.zoom = parser.present<float>("--zoom");
    options.rotation = parser.present<int>("--rotation");
    options.board = parser.present<std::string>("--board");
    options.show_help = parser.get<bool>("--help") || parser.get<bool>("--version");

    if (options.zoom.has_value() && *options.zoom <= 0.0f) {
        std::cerr << "Invalid zoom value: " << *options.zoom << "\n";
        return false;
    }
    if (options.rotation.has_value() && !is_allowed_rotation(*options.rotation)) {
        std::cerr << "Invalid rotation value: " << *options.rotation << " (must be 0, 90, 180, or 270)\n";
        return false;
    }
    if (options.config_file.empty()) {
        std::cerr << "Missing value for --config-file\n";
        return false;
    }
    if (options.control_http_port < 0 || options.control_http_port > 65535) {
        std::cerr << "Invalid --control-http-port value: " << options.control_http_port << " (must be 1-65535)\n";
        return false;
    }
    if (!options.control_http && options.control_http_port != 0) {
        std::cerr << "--control-http-port requires --control-http\n";
        return false;
    }
    if (options.control_http && options.control_http_port == 0) {
        std::cerr << "--control-http requires a non-zero --control-http-port\n";
        return false;
    }

    return true;
}
