#include "station_products.h"

StationProduct station_product_from_key(const std::string &product)
{
    if (product == "bus")
        return StationProduct::BUS;
    if (product == "tram")
        return StationProduct::TRAM;
    if (product == "suburban")
        return StationProduct::SUBURBAN;
    if (product == "subway")
        return StationProduct::SUBWAY;
    if (product == "ferry")
        return StationProduct::FERRY;
    if (product == "express")
        return StationProduct::EXPRESS;
    if (product == "regional")
        return StationProduct::REGIONAL;

    return StationProduct::UNKNOWN;
}
