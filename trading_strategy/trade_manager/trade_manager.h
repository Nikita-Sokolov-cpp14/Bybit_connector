#pragma once

#include <iostream>
#include <functional>
#include <atomic>
#include <optional>

#include "data_structures/order_request.h"
#include "data_structures/order.h"
#include "data_structures/position.h"
#include "trade.h"

class TradeManager {
public:
    struct OrderMainData {
        std::string_view orderId; // 5. ID ордера
        std::string_view orderLinkId; // 6. Клиентский ID
        Side side; // 7. Сторона

        OrderStatus orderStatus; // 8. Статус (Filled/PartiallyFilled/Cancelled)
    };

    TradeManager();

    void setOrderSender(Trade::OrderSender orderSender);
    void setOrder(const OrderMainData &order);
    void setPosition(const PositionHFT &position);

    void makeTrade(const double price, const Side &side);

    bool hasOpenBuyTrade() const;
    bool hasOpenSellTrade() const;

    void closeTrade();

    // Проверить - не противоположный ли сигнал?
    void checkInverseSignal(const Side &side);

    bool hasOpenTrade() const;

    void checkCurrentPrice(const double price);

private:
    std::optional<Side> currentTradeSide_;
    Trade currentTrade_;

    // std::atomic<double> priceOpen_;
    // std::atomic<double> priceDlose_;

    double openPrice;
    double currentPrice;

    std::chrono::_V2::steady_clock::time_point startTrade;
    std::chrono::_V2::steady_clock::time_point timePlaceOpenOrder_;
    std::chrono::_V2::steady_clock::time_point timePlaceCloseOrder_;

    bool waitOpenLimitOrder_;
    bool waitCloseLimitOrder_;
    bool waitCloseMarketOrder_;
    bool waitCancelCloseLimitOrder_;

    void checkMainOrder(const OrderStatus &orderStatus);
    // void checkStopLoss(const OrderStatus &orderStatus);
    // void checkTakeProfit(const OrderStatus &orderStatus);
    void checkCloseLimitOrder(const OrderStatus &orderStatus);
    void checkCloseMarketOrder(const OrderStatus &orderStatus);

    void checkSLTP();
};
