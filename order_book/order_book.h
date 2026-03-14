#pragma once

#include <cstdint>
#include <boost/container/flat_map.hpp>

struct PriceLevel {
    double price;
    double quantity;
    uint64_t timestamp_ns; // наносекундная точность

    PriceLevel() = default;
    PriceLevel(double p, double q, uint64_t ts) noexcept : price(p), quantity(q), timestamp_ns(ts) {
    }
};

class OrderBook {
public:
    using LevelsBids =
            boost::container::flat_map<double, PriceLevel, std::greater<double> >; // bids
    using LevelsAsks = boost::container::flat_map<double, PriceLevel, std::less<double> >;
    // для asks: std::less<double>

    LevelsBids bids_;
    LevelsAsks asks_;

private:
    static constexpr size_t MAX_LEVELS = 1000;

    uint64_t sequence_; // глобальный порядковый номер обновления
    uint64_t last_update_ns_;

public:
    // Применяет snapshot - полная замена стакана
    void apply_snapshot(std::string_view data, uint64_t timestamp_ns);

    // Применяет delta - инкрементальное обновление
    bool apply_delta(std::string_view data, uint64_t timestamp_ns);

    // Быстрый доступ для торговой стратегии
    double get_best_bid() const noexcept {
        return bids_.empty() ? 0.0 : bids_.begin()->second.price;
    }

    double get_best_ask() const noexcept {
        return asks_.empty() ? 0.0 : asks_.begin()->second.price;
    }

    // Для microbenchmarks
    uint64_t get_sequence() const noexcept {
        return sequence_;
    }
};
