#pragma once

#include <iostream>
#include <functional>
#include <atomic>
#include <optional>

#include "data_structures/order_request.h"
#include "trade.h"

class TradeManager {
public:
    TradeManager();

    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;

    void setOrderSender(OrderSender orderSender);

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

    void openTrade();
    void closeTrade(const double endPrice);
};
