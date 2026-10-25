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

uint64_t GetOrderTypeId(const uint64_t orderLinkId);

struct Trade {
    Trade();

    static const uint32_t idOpenLimitOrder = 1;
    static const uint32_t idOpenMarketOrder = 2;
    static const uint32_t idStopLoss = 3;
    static const uint32_t idTakeProfit = 4;
    static const uint32_t idCloseLimitOrder = 5;
    static const uint32_t idCloseMarketOrder = 6;

    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;
    OrderSender orderSender;

    OrderRequest stopLoss;
    OrderRequest takeProfit;
    OrderRequest openLimitOrder;
    OrderRequest openMarketOrder;
    OrderRequest closeLimitOrder;
    OrderRequest closeMarketOrder;
    OrderRequest orderCancel;

    uint64_t tradeNumber;
    uint64_t currentTradeNumber;

    double takeProfitPrice;
    double stopLossPrice;

    bool orderIsPlaced;
    bool orderIsFilled;

    std::unordered_map<uint64_t, OrderStatus> ordersStatus;

    void clearStatuses();

    bool makeTradeByLimitOrder(const double price, const Side &side);
    bool makeTradeByMarketOrder(const Side &side);
    bool makeTPSLOrders(const double price, const Side &side);
    bool makeCloseLimitOrder(const double price, const Side &side);
    bool makeCloseMarketOrder(const Side &side);

    void calcSLTP(double price, const Side &side);

    bool checkOrderStatus();

    bool cancelOrder(const uint64_t orderId);

    bool sendOrder(const OrderRequest &order);

    bool replaceLimitOpenOrder(double price);
};
