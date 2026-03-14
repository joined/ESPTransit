#pragma once

enum class StationProduct {
    UNKNOWN = 0,
    BUS,
    TRAM,
    SUBURBAN,
    SUBWAY,
    FERRY,
    EXPRESS,
    REGIONAL,
};

#include <string>

StationProduct station_product_from_key(const std::string &product);
