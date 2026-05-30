#include "imbalance_indicator.h"
#include "settings.h"

#include <chrono>
#include <cmath>

namespace {

uint64_t countRows;

}

ImbalanceIndicator::ImbalanceIndicator() :
midDisbalance_(settings::averrageDisbalanceCount),
disbalanceAverage_(0.0f),
signal_(Side_Unknown),
countSignal_(0),
sumDisbalance_(0),
loger_("../log_files/imbalance.csv") {
    loger_.recordValue("timestamp");
    loger_.recordValue("mid price");
    loger_.recordValue("disbalance");
    loger_.recordValue("signal");
    loger_.endStr();
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

    // std::cout << disbalanceAverage_ << std::endl;
    //     if (hasSignal()) {
    //         if (signal_.load() == Side_Buy) {
    //             std::cout << "Buy ";
    //         } else {
    //             std::cout << "Sell ";
    //         }
    //         std::cout << disbalanceAverage_ << std::endl;
    //     }

    loger_.recordValue(std::chrono::steady_clock::now().time_since_epoch());
    loger_.recordValue(orderbook.asks.begin()->first);
    loger_.recordValue(disbalance);
    std::string signalStr = "";
    if (signal_.load() == Side_Buy) {
        signalStr = "buy";
    } else if (signal_.load() == Side_Sell) {
        signalStr = "sell";
    }
    loger_.recordValue(signalStr);
    loger_.endStr();
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
    double wi = 1.0;
    const double bestAsk = orderbook.asks.begin()->first;
    const double bestBid = orderbook.bids.begin()->first;
    double distancePct = 0.0;

    for (auto it = orderbook.asks.begin(); it < orderbook.asks.begin() + depthAsks; ++it) {
        distancePct = (it->first - bestAsk) / bestAsk * 100 *
                100; // еще раз умножается на 100, чтобы число было чуть больше
        wi = 1.0 / pow(1.0 + distancePct, 2);
        sumVolAsks += it->second * wi;
    }

    for (auto it = orderbook.bids.begin(); it < orderbook.bids.begin() + depthBids; ++it) {
        distancePct = (bestBid - it->first) / bestBid * 100 *
                100; // еще раз умножается на 100, чтобы число было чуть больше
        wi = 1.0 / pow(1.0 + distancePct, 2);
        sumVolBids += it->second * wi;
    }

    if ((sumVolBids + sumVolAsks) == 0) {
        return 0.f;
    }

    return (sumVolBids - sumVolAsks) / (sumVolBids + sumVolAsks);
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

void ImbalanceIndicator::logData() {
}
