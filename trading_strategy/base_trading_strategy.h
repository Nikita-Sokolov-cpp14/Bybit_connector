#pragma once

#include "data_structures/orderbook.h"
#include "data_structures/public_trade.h"
#include "data_structures/order_request.h"
#include "data_structures/order.h"
#include "data_structures/position.h"
#include "trade_manager/trade_manager.h"

#include <condition_variable>
#include <thread>
#include <iostream>
#include <functional>

class BaseTradingStrategy {
public:
    virtual ~BaseTradingStrategy();

    virtual void setOrderbook(const OrderBook &orderbook) {
        std::cout << "BaseTradingStrategy::setOrderbook" << std::endl;
    }
    virtual void setPublicTradeData(PublicTrade::VectorData &&publicTradeData) {
        std::cout << "BaseTradingStrategy::setPublicTradeData" << std::endl;
    }
    virtual void setOrder(const OrderHFT &order) {
        std::cout << "BaseTradingStrategy::setOrder" << std::endl;
    }
    virtual void setPosition(const PositionHFT &position) {
        std::cout << "BaseTradingStrategy::setPosition" << std::endl;
    }

    // Управление жизненным циклом
    void start();
    void stop();
    bool isRunning() const {
        return running_;
    }

    void setOrderSender(TradeManager::OrderSender orderSender);

protected:
    std::condition_variable dataCV_;
    std::atomic<bool> newData_ {false};
    TradeManager tradeManager_;

    // Чисто виртуальный метод - торговая логика
    virtual void onMarketUpdate() {
        std::cout << "BaseTradingStrategy::onMarketUpdate" << std::endl;
    }

private:
    // Данные
    mutable std::mutex dataMutex_;
    std::atomic<bool> running_ {false};
    std::unique_ptr<std::jthread> strategyThread_;

    void work(); // Главный цикл стратегии
};
