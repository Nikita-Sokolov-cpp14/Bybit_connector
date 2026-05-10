#pragma once

#include "data_structures/orderbook.h"
#include "data_structures/public_trade.h"
#include "data_structures/order_request.h"

#include <condition_variable>
#include <thread>

class ConnectionManager;

class BaseTradingStrategy {
public:
    explicit BaseTradingStrategy(ConnectionManager *connManager);
    virtual ~BaseTradingStrategy();

    virtual void setOrderbook(const OrderBook &orderbook) = 0;
    virtual void setPublicTradeData(PublicTrade::VectorData &&publicTradeData) = 0;

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
    virtual void onMarketUpdate() = 0;

private:
    // Данные
    mutable std::mutex dataMutex_;
    // std::condition_variable dataCV_;
    std::atomic<bool> newData_ {false};
    std::atomic<bool> running_ {false};
    // std::unique_ptr<std::jthread> strategyThread_;

    ConnectionManager *connectionManager_;

    void work(); // Главный цикл стратегии
};
