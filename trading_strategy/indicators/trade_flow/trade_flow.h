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
#include <boost/circular_buffer.hpp>
#include <optional>
#include <numeric>
#include <list>
#include "loger/loger.h"

class TradeFlowIndicator {
public:
    TradeFlowIndicator();

    void setPublicTrade(const PublicTrade &publicTrade);

    bool hasSignal();

    Side getSignal();

    void setMidPrice(double price);

private:
    struct SmallTradeData {
        uint64_t T;
        double v;
        double p;
        PublicTrade::TickDirection L;
        bool BT;
        bool RPI;
        uint64_t seq;
    };

    // Окно на 300 мс примерно. Короткое.
    std::list<SmallTradeData> shortWindow_;
    // Окно на 2 с примерно. Базовое.
    std::list<SmallTradeData> baseWindow_;
    std::vector<double> netFlowIntervalData_;
    std::vector<double> sumVolBuy_;
    std::vector<double> sumVolSell_;
    double netFlowShort_;
    double mu_;
    double sigma_;
    std::atomic<Side> signal_;
    std::atomic<double> midPrice_;
    double zScore_;
    Logger loger_;

    void checkActualityTime();
    void calculateShort();
    void calculateBase();

    void checkSignal();

    bool isBuy(const SmallTradeData &trade);

    void logData();
};
