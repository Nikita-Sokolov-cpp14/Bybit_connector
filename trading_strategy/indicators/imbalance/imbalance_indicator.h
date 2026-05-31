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
#include "loger/loger.h"

class ImbalanceIndicator {
public:
    ImbalanceIndicator();

    void setOrderbook(const OrderBook &orderbook);

    bool hasSignal();

    Side getSignal();

private:
    boost::circular_buffer<double> imbalanceSrorage_;
    double disbalanceAverage_;
    double disbalanceRecent_;
    double disbalancePrev_;
    std::atomic<Side> signal_;

    Logger loger_;

    double calcDisbalance(const OrderBook &orderbook) ;

    void checkSignal();

    double getSKO();
};
