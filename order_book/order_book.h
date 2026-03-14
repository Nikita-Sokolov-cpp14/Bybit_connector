#pragma once

#include <cstdint>
#include <boost/container/flat_map.hpp>

class OrderBook {
public:
    using LevelsBids = boost::container::flat_map<double, double, std::greater<double> >;
    using LevelsAsks = boost::container::flat_map<double, double, std::less<double> >;

    std::string_view topic;
    uint64_t ts;
    std::string_view s;
    LevelsBids bids;
    LevelsAsks asks;
    uint64_t u;
    uint64_t seq;
    uint64_t cts;

    void clearLevels();
};
