#include "imbalance_indicator.h"
#include "settings.h"

#include <chrono>
#include <cmath>

namespace {

uint64_t countRows;

}

ImbalanceIndicator::ImbalanceIndicator() :
imbalanceSrorage_(settings::averrageDisbalanceCount),
disbalanceAverage_(0.0f),
signal_(Side_Unknown),
loger_("../log_files/imbalance.csv") {
    loger_.recordValue("timestamp_ns");
    loger_.recordValue("mid_price");
    loger_.recordValue("DW_OBI");
    loger_.recordValue("OBI_recent");
    loger_.recordValue("OBI_prev");
    loger_.recordValue("z_score");
    loger_.recordValue("signal");
    loger_.endStr();
}

void ImbalanceIndicator::setOrderbook(const OrderBook &orderbook) {
    const double disbalance = calcDisbalance(orderbook);
    imbalanceSrorage_.push_back(disbalance);

    if (imbalanceSrorage_.size() < imbalanceSrorage_.capacity()) {
        signal_.store(Side_Unknown);
        return;
    }

    // recent: индексы [size-10, size-1] — последние 10 элементов
    double sumDisbalanceRecent = 0.0;
    for (size_t i = imbalanceSrorage_.size() - 10; i < imbalanceSrorage_.size(); ++i) {
        sumDisbalanceRecent += imbalanceSrorage_[i];
    }

    // prev: индексы [size-20, size-11] — предыдущие 10 элементов
    double sumDisbalancePrev = 0.0;
    for (size_t i = imbalanceSrorage_.size() - 20; i < imbalanceSrorage_.size() - 10; ++i) {
        sumDisbalancePrev += imbalanceSrorage_[i];
    }

    double sumDisbalance = 0.0;
    for (size_t i = 0; i < imbalanceSrorage_.size(); ++i) {
        sumDisbalance += imbalanceSrorage_[i];
    }

    disbalanceAverage_ = sumDisbalance / imbalanceSrorage_.size();
    disbalanceRecent_ = sumDisbalanceRecent / 10;
    disbalancePrev_ = sumDisbalancePrev / 10;

    loger_.recordValue(std::chrono::steady_clock::now().time_since_epoch().count());
    loger_.recordValue(orderbook.asks.begin()->first);
    loger_.recordValue(disbalance);

    checkSignal();
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

double ImbalanceIndicator::calcDisbalance(const OrderBook &orderbook) {
    double sumVolAsks = 0;
    double sumVolBids = 0;

    const size_t depthAsks = std::min(settings::disbalanceDepthCalc, orderbook.asks.size());
    const size_t depthBids = std::min(settings::disbalanceDepthCalc, orderbook.bids.size());
    double wi = 1.0;
    const double bestAsk = orderbook.asks.begin()->first;
    const double bestBid = orderbook.bids.begin()->first;
    double distancePct = 0.0;
    const double scale = 0.01;

    for (auto it = orderbook.asks.begin(); it < orderbook.asks.begin() + depthAsks; ++it) {
        distancePct = (it->first - bestAsk) / bestAsk * 100;
        wi = 1.0 / pow(1.0 + distancePct / scale, 2);
        sumVolAsks += it->second * wi;
    }

    for (auto it = orderbook.bids.begin(); it < orderbook.bids.begin() + depthBids; ++it) {
        distancePct = (bestBid - it->first) / bestBid * 100;
        wi = 1.0 / pow(1.0 + distancePct / scale, 2);
        sumVolBids += it->second * wi;
    }

    if ((sumVolBids + sumVolAsks) == 0) {
        return 0.f;
    }

    return (sumVolBids - sumVolAsks) / (sumVolBids + sumVolAsks);
}

void ImbalanceIndicator::checkSignal() {
    double zNormal = (disbalanceRecent_ - disbalancePrev_) / getSKO();
    double absDeltaImbalance = std::fabs(disbalanceRecent_ - disbalancePrev_);

    loger_.recordValue(disbalanceRecent_);
    loger_.recordValue(disbalancePrev_);
    loger_.recordValue(zNormal);
    std::string signalStr = "";
    if (signal_.load() == Side_Buy) {
        signalStr = "buy";
    } else if (signal_.load() == Side_Sell) {
        signalStr = "sell";
    } else {
        signalStr = "unknown";
    }
    loger_.recordValue(signalStr);
    loger_.endStr();

    // std::cout << "zNormal " << zNormal << std::endl;

    if (absDeltaImbalance < 0.05) {
        signal_.store(Side_Unknown);
        return;
    }

    // std::cout << zNormal << std::endl;

    if (zNormal >= settings::countSigmaBuy) {
        signal_.store(Side_Buy);
    } else if (zNormal <= settings::countSigmaSell) {
        signal_.store(Side_Sell);
    } else {
        signal_.store(Side_Unknown);
    }
}

double ImbalanceIndicator::getSKO() {
    double sumDeltaSquare = 0.0;

    for (const auto &imbalance : imbalanceSrorage_) {
        sumDeltaSquare += pow(imbalance - disbalanceAverage_, 2);
    }

    return sqrt(sumDeltaSquare / (imbalanceSrorage_.size() - 1));
}
