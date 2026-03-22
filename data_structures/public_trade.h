#pragma once

#include <cstdint>
#include <vector>
#include <string_view>

class PublicTrade {
public:
    enum TickDirection {
        TickDirection_Unknown = 0,
        TickDirection_PlusTick,
        TickDirection_MinusTick,
        TickDirection_ZeroPlusTick,
        TickDirection_ZeroMinusTick,
    };

    struct Data {
        uint64_t T;
        std::string_view s;
        std::string_view S;
        double v;
        double p;
        TickDirection L;
        std::string_view i;
        bool BT;
        bool RPI;
        uint64_t seq;
    };

    std::string_view pairStr;
    uint64_t ts;
    std::vector<Data> data;

    void clearData();

    void print();
};
