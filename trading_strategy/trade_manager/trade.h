#pragma once

#include <iostream>
#include <functional>
#include <atomic>
#include <optional>
#include <cstdint>
#include <chrono>
#include <unordered_map>

#include "data_structures/order_request.h"
#include "data_structures/order.h"

struct Trade {
    Trade();

    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;
    OrderSender orderSender;

    OrderRequest orderOpenTrade;
    OrderRequest stopLoss;
    OrderRequest takeProfit;
    OrderRequest openLimitOrder;
    OrderRequest closeLimitOrder;
    OrderRequest orderCancel;

    uint64_t tradeNumber;
    uint64_t currentTradeNumber;

    double takeProfitPrice;
    double stopLossPrice;

    bool orderIsPlaced;
    bool orderIsFilled;

    std::unordered_map<uint64_t, OrderStatus> ordersStatus;

    void clearStatuses();

    void makeTrade(const double price, const Side &side);
    bool makeTradeByLimitOrder(const double price, const Side &side);
    bool makeTPSLOrders(const double price, const Side &side);
    bool makeCloseLimitOrder(const double price, const Side &side);

    void calcSLTP(double price, const Side &side);

    bool checkOrderStatus();

    bool cancelOrder(const uint64_t orderId);

    bool sendOrder(const OrderRequest &order);
};
