#include "http_util.h"

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

std::string url_encode(const char *str)
{
    std::string encoded;
    if (!str)
        return encoded;

    while (*str) {
        char c = *str++;
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            encoded += buf;
        }
    }
    return encoded;
}

std::vector<std::string> extract_stop_ids(const char *station_ids)
{
    std::string raw(station_ids ? station_ids : "");
    if (raw.empty()) {
        return {};
    }

    std::vector<std::string> ids;
    size_t start = 0;
    while (start <= raw.size()) {
        size_t comma = raw.find(',', start);
        std::string token = comma == std::string::npos ? raw.substr(start) : raw.substr(start, comma - start);
        if (!token.empty()) {
            ids.push_back(token);
        }

        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    return ids;
}
