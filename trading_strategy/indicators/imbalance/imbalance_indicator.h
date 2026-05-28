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

class ImbalanceIndicator {
public:
    ImbalanceIndicator();

    void setOrderbook(const OrderBook &orderbook);

    bool hasSignal();

    Side getSignal();

private:
    boost::circular_buffer<double> midDisbalance_;
    std::atomic<float> disbalanceAverage_;
    std::atomic<Side> signal_;
    std::atomic<size_t> countSignal_;
    float sumDisbalance_;

    float calcDisbalance(const OrderBook &orderbook) ;

    void checkSignal();
};
