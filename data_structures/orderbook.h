#pragma once

#include <cstdint>
#include <boost/container/flat_map.hpp>
#include <string_view>
#include <mutex>

class OrderBook {
public:
    using LevelsBids = boost::container::flat_map<double, double, std::greater<double> >;
    using LevelsAsks = boost::container::flat_map<double, double, std::less<double> >;

    std::string_view topic; // TODO поменять на глубину и название пары
    uint64_t ts;
    std::string_view s;
    LevelsBids bids;
    LevelsAsks asks;
    uint64_t u;
    uint64_t seq;
    uint64_t cts;

    //! TODO: Стакан обновляется не чаще 1000 раз в секунду.
    // Чтобы пока не заморачиваться сделан mutex
    std::mutex mt;

    OrderBook();

    void clearLevels();

    void print();
};
