#pragma once

#include <optional>
#include <string>
#include <vector>

struct CliOptions {
    bool show_help = false;
    bool mock_http{};
    bool control_http = false;
    int control_http_port = 0;
    std::optional<float> zoom;
    std::optional<int> rotation;
    std::optional<std::string> board;
    std::string config_file;
};

struct BoardInfo {
    std::string name;
    int width;
    int height;

    bool is_native_landscape() const { return width > height; }
};

bool parse_args(int argc, char **argv, CliOptions &options);
bool load_boards(std::vector<BoardInfo> &boards);
const BoardInfo *find_board(const std::vector<BoardInfo> &boards, const std::string &name);
std::string available_board_names(const std::vector<BoardInfo> &boards);
