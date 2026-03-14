#pragma once

#include <string>
#include <vector>

// URL-encode a string
std::string url_encode(const char *str);

// Split a comma-separated list of plain stop IDs.
// "900017101,900023301" -> ["900017101", "900023301"]
std::vector<std::string> extract_stop_ids(const char *station_ids);
