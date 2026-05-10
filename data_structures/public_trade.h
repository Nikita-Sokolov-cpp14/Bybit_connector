#pragma once

#include <cstdint>
#include <vector>
#include <string_view>
#include <mutex>

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

    using VectorData = std::vector<Data>;

    std::string_view pairStr;
    uint64_t ts;
    VectorData data;

    //! TODO: Стакан обновляется не чаще 1000 раз в секунду.
    // Чтобы пока не заморачиваться сделан mutex
    std::mutex mt;

    void clearData();

    void print() const;
};
