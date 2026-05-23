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
    TradeManager();

    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;

    void setOrderSender(OrderSender orderSender);
    void setOrder(const OrderHFT &order);
    void setPosition(const PositionHFT &position);

    void makeTrade(const double price, const Side &side);

    bool hasOpenBuyTrade() const;
    bool hasOpenSellTrade() const;

    void closeBuyTrade();
    void closeSellTrade();

    // Проверить - не противоположный ли сигнал?
    void checkInverseSignal(const Side &side);

    bool hasOpenTrade() const;

    void checkCurrentPrice(const double price);

private:
    OrderSender orderSender_;

    std::optional<Side> currentTradeSide_;
    Trade currentTrade_;

    // std::atomic<double> priceOpen_;
    // std::atomic<double> priceDlose_;

    double openPrice;
    double currentPrice;

    std::chrono::_V2::steady_clock::time_point startTrade;
    std::chrono::_V2::steady_clock::time_point timePlaceOrder_;

    bool waitOpenLimitOrder_;

    void openTrade();
    void closeTrade(const double endPrice);

    void checkMainOrder(const OrderStatus &orderStatus);
    void checkStopLoss(const OrderStatus &orderStatus);
    void checkTakeProfit(const OrderStatus &orderStatus);
    void checkCloseOrder(const OrderStatus &orderStatus);
};
