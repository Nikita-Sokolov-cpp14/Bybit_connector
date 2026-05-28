#include "imbalance_indicator.h"
#include "settings.h"

ImbalanceIndicator::ImbalanceIndicator() :
midDisbalance_(settings::averrageDisbalanceCount),
disbalanceAverage_(0.0f),
signal_(Side_Unknown),
countSignal_(0),
sumDisbalance_(0) {
}

void ImbalanceIndicator::setOrderbook(const OrderBook &orderbook) {
    const float disbalance = calcDisbalance(orderbook);

    sumDisbalance_ += disbalance;
    if (midDisbalance_.size() == midDisbalance_.capacity()) {
        sumDisbalance_ -= midDisbalance_.front();
    }
    midDisbalance_.push_back(disbalance);
    disbalanceAverage_.store(sumDisbalance_ / midDisbalance_.size());

    checkSignal();

    // if (hasSignal()) {
    //     if (signal_.load() == Side_Buy) {
    //         std::cout << "Buy ";
    //     } else {
    //         std::cout << "Sell ";
    //     }
    //     std::cout << disbalanceAverage_ << std::endl;
    // }
}

bool ImbalanceIndicator::hasSignal() {
    if (signal_.load() != Side_Unknown) {
        return true;
    }

    return false;
}

Side ImbalanceIndicator::getSignal() {
    return signal_.load();
}

float ImbalanceIndicator::calcDisbalance(const OrderBook &orderbook) {
    float sumVolAsks = 0;
    float sumVolBids = 0;

    const size_t depthAsks = std::min(settings::disbalanceDepthCalc, orderbook.asks.size());
    const size_t depthBids = std::min(settings::disbalanceDepthCalc, orderbook.bids.size());

    for (auto it = orderbook.asks.begin(); it < orderbook.asks.begin() + depthAsks; ++it) {
        sumVolAsks += it->second;
    }

    for (auto it = orderbook.bids.begin(); it < orderbook.bids.begin() + depthBids; ++it) {
        sumVolBids += it->second;
    }

    if (sumVolAsks == 0) {
        return 1.0;
    }

    return sumVolBids / sumVolAsks;
}

void ImbalanceIndicator::checkSignal() {
    if (midDisbalance_.size() < midDisbalance_.capacity()) {
        signal_.store(Side_Unknown);
        return;
    }

    if (disbalanceAverage_ >= settings::buyDisbalance) {
        if (signal_.load() == Side_Buy) {
            countSignal_++;
        } else if (signal_.load() == Side_Sell) {
            countSignal_.store(1);
        } else {
            countSignal_ = std::max(0UL, (countSignal_ - 1));
        }
        signal_.store(Side_Buy);
    } else if (disbalanceAverage_ <= settings::sellDisbalance) {
        if (signal_.load() == Side_Sell) {
            countSignal_++;
        } else if (signal_.load() == Side_Buy) {
            countSignal_.store(1);
        } else {
            countSignal_ = std::max(0UL, (countSignal_ - 1));
        }
        signal_.store(Side_Sell);
    } else {
        signal_.store(Side_Unknown);
    }
}
