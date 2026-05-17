#pragma once

#include <iostream>
#include <functional>
#include <atomic>
#include <optional>

#include "data_structures/order_request.h"

class TradeManager {
public:
    TradeManager();

    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;

    void setOrderSender(OrderSender orderSender);

    void makeBuyTrade();
    void makeSellTrade();

    bool hasOpenBuyTrade() const;
    bool hasOpenSellTrade() const;

    void closeBuyTrade();
    void closeSellTrade();

    // Проверить - не противоположный ли сигнал?
    void checkInverseSignal(const Side &side);

    bool hasOpenTrade() const;

private:
    OrderSender orderSender_;

    std::optional<Side> currentTradeSide_;

    std::atomic<double> priceOpen_;
    std::atomic<double> priceDlose_;

    void openTrade();
    void closeTrade();
};
