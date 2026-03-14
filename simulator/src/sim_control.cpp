#include "sim_control.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>

#include <ArduinoJson.h>
#include <httplib.h>

#include "bsp/display.h"

#if !defined(LV_USE_OBJ_NAME) || (LV_USE_OBJ_NAME != 1)
#error "LV_USE_OBJ_NAME must be set to 1 for simulator control."
#endif

namespace {

struct control_command_result {
    bool ok;
    std::string code;
    std::string message;
    bool should_quit;
};

class inline_task_queue final : public httplib::TaskQueue {
  public:
    bool enqueue(std::function<void()> fn) override
    {
        fn();
        return true;
    }

    void shutdown() override {}
};

std::mutex s_control_command_mutex;
std::string s_http_host = "127.0.0.1";
int s_http_port = 0;

control_command_result control_ok(const std::string &message, bool should_quit = false)
{
    return {true, "ok", message, should_quit};
}

control_command_result control_error(const std::string &code, const std::string &message)
{
    return {false, code, message, false};
}

std::string trim_copy(const std::string &input)
{
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }

    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }

    return input.substr(start, end - start);
}

bool parse_uint32_arg(const std::string &raw, uint32_t &value_out)
{
    if (raw.empty()) {
        return false;
    }

    errno = 0;
    char *end = nullptr;
    unsigned long parsed = std::strtoul(raw.c_str(), &end, 10);
    if (errno != 0 || end == raw.c_str() || (end && *end != '\0') || parsed > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    value_out = static_cast<uint32_t>(parsed);
    return true;
}

bool parse_json_uint32(JsonVariantConst value, uint32_t &value_out)
{
    if (value.isNull()) {
        return false;
    }
    if (value.is<uint32_t>()) {
        value_out = value.as<uint32_t>();
        return true;
    }
    if (value.is<unsigned long long>()) {
        unsigned long long parsed = value.as<unsigned long long>();
        if (parsed <= std::numeric_limits<uint32_t>::max()) {
            value_out = static_cast<uint32_t>(parsed);
            return true;
        }
        return false;
    }
    if (value.is<long long>()) {
        long long parsed = value.as<long long>();
        if (parsed >= 0 && static_cast<unsigned long long>(parsed) <= std::numeric_limits<uint32_t>::max()) {
            value_out = static_cast<uint32_t>(parsed);
            return true;
        }
        return false;
    }
    if (value.is<const char *>()) {
        return parse_uint32_arg(value.as<const char *>(), value_out);
    }
    return false;
}

bool get_required_string_arg(JsonObjectConst args, const char *key, std::string &out)
{
    JsonVariantConst value = args[key];
    if (!value.is<const char *>()) {
        return false;
    }
    out = value.as<const char *>();
    return true;
}

bool control_wait_for_id(const char *id, uint32_t timeout_ms)
{
    if (!id || id[0] == '\0') {
        return false;
    }

    if (sim_object_exists_by_name(id)) {
        return true;
    }
    if (timeout_ms == 0) {
        return false;
    }

    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ticks == 0) {
        timeout_ticks = 1;
    }
    TickType_t start = xTaskGetTickCount();
    const TickType_t poll_ticks = pdMS_TO_TICKS(20);

    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        vTaskDelay(poll_ticks > 0 ? poll_ticks : 1);
        if (sim_object_exists_by_name(id)) {
            return true;
        }
    }

    return false;
}

bool ensure_parent_directory(const std::string &path, std::string &error_out)
{
    std::filesystem::path file_path(path);
    if (file_path.empty() || file_path.parent_path().empty()) {
        return true;
    }

    std::error_code dir_error;
    std::filesystem::create_directories(file_path.parent_path(), dir_error);
    if (dir_error) {
        error_out = "failed to create directory '" + file_path.parent_path().string() + "': " + dir_error.message();
        return false;
    }
    return true;
}

control_command_result execute_control_command(const std::string &command_raw, JsonObjectConst args)
{
    const std::string command = trim_copy(command_raw);
    if (command.empty()) {
        return control_error("invalid_request", "missing cmd");
    }

    if (command == "wait_for_id") {
        std::string id;
        if (!get_required_string_arg(args, "id", id) || id.empty()) {
            return control_error("invalid_args", "wait_for_id requires args.id");
        }

        uint32_t timeout_ms = 5000;
        JsonVariantConst timeout_arg = args["timeout_ms"];
        if (!timeout_arg.isNull() && !parse_json_uint32(timeout_arg, timeout_ms)) {
            return control_error("invalid_args", "wait_for_id args.timeout_ms must be an integer >= 0");
        }

        if (control_wait_for_id(id.c_str(), timeout_ms)) {
            return control_ok("wait_for_id " + id);
        }
        return control_error("timeout", "wait_for_id timeout: " + id);
    }

    if (command == "click_id") {
        std::string id;
        if (!get_required_string_arg(args, "id", id) || id.empty()) {
            return control_error("invalid_args", "click_id requires args.id");
        }

        int result = sim_click_object_by_name(id.c_str());
        if (result == SIM_CLICK_OK) {
            return control_ok("click_id " + id);
        }
        if (result == SIM_CLICK_NOT_VISIBLE) {
            return control_error("target_not_visible", "click_id target not visible (scrolled out of view): " + id);
        }
        return control_error("target_not_found", "click_id target_not_found: " + id);
    }

    if (command == "scroll_to_view_id") {
        std::string id;
        if (!get_required_string_arg(args, "id", id) || id.empty()) {
            return control_error("invalid_args", "scroll_to_view_id requires args.id");
        }

        if (sim_scroll_to_view_by_name(id.c_str())) {
            return control_ok("scroll_to_view_id " + id);
        }
        return control_error("target_not_found", "scroll_to_view_id target_not_found: " + id);
    }

    if (command == "type_text") {
        std::string text;
        if (!get_required_string_arg(args, "text", text)) {
            return control_error("invalid_args", "type_text requires args.text");
        }

        if (sim_type_text_with_test_keys(text.c_str())) {
            return control_ok("type_text");
        }
        return control_error("execution_failed", "type_text failed");
    }

    if (command == "set_dropdown_id") {
        std::string id;
        if (!get_required_string_arg(args, "id", id) || id.empty()) {
            return control_error("invalid_args", "set_dropdown_id requires args.id");
        }

        uint32_t selected_index = 0;
        if (!parse_json_uint32(args["index"], selected_index)) {
            return control_error("invalid_args", "set_dropdown_id requires args.index as integer >= 0");
        }
        if (selected_index > std::numeric_limits<uint16_t>::max()) {
            return control_error("invalid_args", "set_dropdown_id args.index out of range");
        }

        if (sim_set_dropdown_selected_by_name(id.c_str(), static_cast<uint16_t>(selected_index))) {
            return control_ok("set_dropdown_id " + id + " " + std::to_string(selected_index));
        }
        return control_error("execution_failed", "set_dropdown_id failed: " + id);
    }

    if (command == "screenshot") {
        std::string path;
        if (!get_required_string_arg(args, "path", path) || path.empty()) {
            return control_error("invalid_args", "screenshot requires args.path");
        }

        std::string dir_error;
        if (!ensure_parent_directory(path, dir_error)) {
            return control_error("invalid_path", "screenshot " + dir_error);
        }

        if (sim_capture_screenshot(path.c_str())) {
            return control_ok("screenshot " + path);
        }
        return control_error("execution_failed", "screenshot failed: " + path);
    }

    if (command == "sleep_ms") {
        uint32_t sleep_ms = 0;
        if (!parse_json_uint32(args["milliseconds"], sleep_ms)) {
            return control_error("invalid_args", "sleep_ms requires args.milliseconds as integer >= 0");
        }

        TickType_t delay_ticks = pdMS_TO_TICKS(sleep_ms);
        vTaskDelay(delay_ticks > 0 ? delay_ticks : 1);
        return control_ok("sleep_ms " + std::to_string(sleep_ms));
    }

    if (command == "quit") {
        return control_ok("quit", true);
    }

    return control_error("unknown_command", "unknown command: " + command);
}

std::string serialize_health_response()
{
    JsonDocument doc;
    doc["ok"] = true;
    doc["service"] = "sim_control";

    std::string payload;
    serializeJson(doc, payload);
    return payload;
}

std::string serialize_control_response(const control_command_result &result, const std::string &command)
{
    JsonDocument doc;
    doc["ok"] = result.ok;

    if (result.ok) {
        JsonObject response = doc["result"].to<JsonObject>();
        response["cmd"] = command;
        response["code"] = result.code;
        response["message"] = result.message;
    } else {
        JsonObject response = doc["error"].to<JsonObject>();
        response["cmd"] = command;
        response["code"] = result.code;
        response["message"] = result.message;
    }

    std::string payload;
    serializeJson(doc, payload);
    return payload;
}

void write_bad_request_response(httplib::Response &res, const std::string &code, const std::string &message)
{
    res.status = 400;
    res.set_content(serialize_control_response(control_error(code, message), ""), "application/json");
}

void control_http_task(void *arg)
{
    (void)arg;

    httplib::Server server;
    server.new_task_queue = [] { return new inline_task_queue(); };

    server.Get("/health", [](const httplib::Request &, httplib::Response &res) {
        res.status = 200;
        res.set_content(serialize_health_response(), "application/json");
    });

    server.Post("/control", [&server](const httplib::Request &req, httplib::Response &res) {
        JsonDocument req_doc;
        DeserializationError parse_error = deserializeJson(req_doc, req.body);
        if (parse_error != DeserializationError::Ok) {
            write_bad_request_response(res, "invalid_json", "Invalid JSON body");
            return;
        }

        JsonObjectConst root = req_doc.as<JsonObjectConst>();
        if (root.isNull()) {
            write_bad_request_response(res, "invalid_request", "Request body must be an object");
            return;
        }
        JsonVariantConst cmd_value = root["cmd"];
        if (!cmd_value.is<const char *>()) {
            write_bad_request_response(res, "invalid_request", "Request requires string field 'cmd'");
            return;
        }

        const std::string command = cmd_value.as<const char *>();
        JsonVariantConst args_value = root["args"];
        JsonObjectConst args = args_value.as<JsonObjectConst>();
        if (!args_value.isNull() && args.isNull()) {
            write_bad_request_response(res, "invalid_request", "Field 'args' must be an object");
            return;
        }
        control_command_result result = {};
        {
            std::lock_guard<std::mutex> lock(s_control_command_mutex);
            result = execute_control_command(command, args);
        }

        res.status = 200;
        res.set_content(serialize_control_response(result, command), "application/json");
        if (result.should_quit) {
            server.stop();
        }
    });

    if (!server.bind_to_port(s_http_host, s_http_port)) {
        std::cerr << "Failed to bind HTTP control server on " << s_http_host << ":" << s_http_port << "\n";
        std::_Exit(1);
    }

    if (!server.listen_after_bind()) {
        std::cerr << "HTTP control server stopped unexpectedly\n";
        std::_Exit(1);
    }

    // Avoid global/static teardown across worker threads on shutdown.
    std::_Exit(0);
}

} // namespace

bool sim_control_start_http_task(const char *host, int port)
{
    if (!host || host[0] == '\0' || port <= 0 || port > 65535) {
        return false;
    }

    s_http_host = host;
    s_http_port = port;
    return xTaskCreate(control_http_task, "sim_http_ctl", 16384, nullptr, 1, nullptr) == pdPASS;
}
