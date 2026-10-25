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

    static const uint64_t multiplier = 10;

    /**
     * @brief Тип ордера внутри сделки.
     */
    enum OrderType {
        OrderType_Unknown = 0,
        OrderType_OpenLimitOrder,
        OrderType_OpenMarketOrder,
        OrderType_CloseLimitOrder,
        OrderType_CloseMarketOrder
    };

    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;
    OrderSender orderSender;

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
    bool makeCloseLimitOrder(const double price, const Side &side);
    bool makeCloseMarketOrder(const Side &side);

    void calcSLTP(double price, const Side &side);

    bool checkOrderStatus();

    bool cancelOrder(const uint64_t orderId);

    bool sendOrder(const OrderRequest &order);

    bool replaceLimitOpenOrder(double price);

    OrderType getOrderType(const uint64_t orderLinkId);

    uint64_t makeReqId(const OrderType &orderType);
};
