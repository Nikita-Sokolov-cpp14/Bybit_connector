#pragma once

#include <iostream>
#include <functional>
#include <atomic>
#include <optional>
#include <cstdint>
#include <chrono>

#include "data_structures/order_request.h"

struct Trade {
    Trade();

    OrderRequest orderOpenTrade;
    OrderRequest stopLoss;
    OrderRequest takeProfit;

    uint32_t tradeNumber;

    double takeProfitPrice;
    double stopLossPrice;

    void makeTrade(const double price, const Side &side);
};
