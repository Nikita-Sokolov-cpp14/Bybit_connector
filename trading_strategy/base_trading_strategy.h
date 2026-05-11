#pragma once

#include "data_structures/orderbook.h"
#include "data_structures/public_trade.h"
#include "data_structures/order_request.h"

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

    // Управление жизненным циклом
    void start();
    void stop();
    bool isRunning() const {
        return running_;
    }

    // Отправка ордера
    bool sendOrder(const OrderRequest &request);

protected:
    // Чисто виртуальный метод - торговая логика
    virtual void onMarketUpdate() {
        std::cout << "BaseTradingStrategy::onMarketUpdate" << std::endl;
    }

private:
    // Функциональный объект для отправки ордеров
    using OrderSender = std::function<bool(const OrderRequest &)>;

    // Данные
    mutable std::mutex dataMutex_;
    std::condition_variable dataCV_;
    std::atomic<bool> newData_ {false};
    std::atomic<bool> running_ {false};
    std::unique_ptr<std::jthread> strategyThread_;

    void work(); // Главный цикл стратегии
};
