#include "imbalance_and_large.h"

#include <iostream>

#include "settings.h"

ImbalanceAndLarge::ImbalanceAndLarge() :
disbalance_(0),
orderbookIsUpdate_(false),
publicTradeIsUpdate_(false),
midPrices_(settings::historyMidPriceSize),
publicTrades_(settings::historyPublicTradeSize) {
}

void ImbalanceAndLarge::setOrderbook(const OrderBook &orderbook) {
    calcDisbalance(orderbook);
    const double midPrice = (orderbook.asks.begin()->first + orderbook.bids.begin()->first) / 2.0;
    midPrices_.push_back(midPrice);
    orderbookIsUpdate_.store(true);
}

void ImbalanceAndLarge::setPublicTradeData(PublicTrade::VectorData &&publicTradeData) {
    //! TODO: После этой опреации у publicTrade больше нет данных.
    publicTrades_.push_back(std::move(publicTradeData));
    publicTradeIsUpdate_.store(true);
}

void ImbalanceAndLarge::calcDisbalance(const OrderBook &orderbook) {
    double sumVolAsks = 0;
    double sumVolBids = 0;

    const size_t depthAsks = std::min(settings::disbalanceDepthCalc, orderbook.asks.size());
    const size_t depthBids = std::min(settings::disbalanceDepthCalc, orderbook.bids.size());

    for (auto it = orderbook.asks.begin(); it < orderbook.asks.begin() + depthAsks; ++it) {
        sumVolAsks += it->second;
    }

    for (auto it = orderbook.bids.begin(); it < orderbook.bids.begin() + depthBids; ++it) {
        sumVolBids += it->second;
    }

    disbalance_.store(sumVolBids / sumVolAsks);
}

void ImbalanceAndLarge::onMarketUpdate() {
}
