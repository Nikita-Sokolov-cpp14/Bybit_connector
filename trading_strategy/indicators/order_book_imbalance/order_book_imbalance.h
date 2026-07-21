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
#include <atomic>
#include <array>
#include <boost/circular_buffer.hpp>
#include <optional>
#include <numeric>
#include "loger/loger.h"

class OrderBookImbalance {
public:
    OrderBookImbalance();

    void setOrderbook(const OrderBook &orderbook);

    bool hasSignal();

    Side getSignal();

private:
    boost::circular_buffer<double> aggObiStorage_;
    double disbalanceAverage_;
    std::atomic<Side> signal_;
    size_t indexAgregateData_;
    double emaObi_;
    double emaObiPrev_;
    double emaRecent_;
    double emaRecentT_1_;
    double smaPrev_;
    double sigma_;
    double dwObi_;
    double midPrice_;
    double emaVariance_; // EMA дисперсии
    bool emaInitialized_; // Флаг инициализации
    double emaMean_;

    Logger loger_;

    double calcDisbalance(const OrderBook &orderbook);

    void checkSignal();

    void getSKO();

    void updateSigmaEMA();

    void logData();
};
