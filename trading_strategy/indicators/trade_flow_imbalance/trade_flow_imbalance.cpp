#include "trade_flow_imbalance.h"
#include "settings.h"

#include <chrono>
#include <iostream>
#include <cmath>

namespace {

// шаг выборки, мс
const size_t step = 300;
const size_t countIntervals = settings::baseTime.count() / step;
const double minSigma = 0.2;
const double maxSigma = 1.2;
// const size_t maxShortWinSize = 200;
// const size_t minShortWinSize = 0;
const double minVolume = 0.05;

const size_t nEff = 100;
const double alpha = 2.0 / (nEff + 1.0);

} // namespace

TradeFlowImbalance::TradeFlowImbalance() :
signal_(Side_Unknown),
netFlowShort_(0.0),
mu_(0.0),
sigma_(0.0),
sumVolBuy_(),
sumVolSell_(),
midPrice_(0.0),
zScore_(0.0),
loger_("../log_files/trade_flow.csv") {
    loger_.recordValue("ts");
    loger_.recordValue("mid_price");
    loger_.recordValue("netFlow");
    loger_.recordValue("mu");
    loger_.recordValue("sigma");
    loger_.recordValue("z_score");
    loger_.recordValue("signal");
    loger_.recordValue("shortWin");
    loger_.recordValue("baseWin");
    loger_.recordValue("dataSize");
    loger_.endStr();
}

void TradeFlowImbalance::setPublicTrade(const PublicTrade &publicTrade) {
    for (const auto &trade : publicTrade.data) {
        SmallTradeData tradeData;
        tradeData.BT = trade.BT;
        tradeData.L = trade.L;
        tradeData.p = trade.p;
        tradeData.RPI = trade.RPI;
        tradeData.seq = trade.seq;
        tradeData.T = trade.T;
        tradeData.v = trade.v;
        // std::cout << "TradeFlowImbalance::setPublicTrade " << tradeData.T << " "
        //           << std::chrono::duration_cast<std::chrono::milliseconds>(
        //                      std::chrono::system_clock::now().time_since_epoch()).count()
        //           << std::endl;

        if (tradeData.v < minVolume) {
            continue;
        }

        shortWindow_.push_back(tradeData);
        baseWindow_.push_back(tradeData);
    }

    checkActualityTime();

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                           .count();

    calculateShort();
    calculateBase();
    checkSignal();

    logData();
}

bool TradeFlowImbalance::hasSignal() {
    if (signal_.load() != Side_Unknown) {
        return true;
    }

    return false;
}

Side TradeFlowImbalance::getSignal() {
    return signal_.load();
}

void TradeFlowImbalance::setMidPrice(double price) {
    midPrice_.store(price);
}

void TradeFlowImbalance::checkActualityTime() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());

    for (auto it = shortWindow_.begin(); it != shortWindow_.end();) {
        if ((*it).T < (now - settings::shortTime).count()) {
            it = shortWindow_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = baseWindow_.begin(); it != baseWindow_.end();) {
        if ((*it).T < (now - settings::baseTime).count()) {
            it = baseWindow_.erase(it);
        } else {
            ++it;
        }
    }
}

void TradeFlowImbalance::calculateShort() {
    double sumVolBuy = 0.0;
    double sumVolSell = 0.0;
    double total = 0.0;

    for (const auto &trade : shortWindow_) {
        if (isBuy(trade)) {
            sumVolBuy += trade.v;
        } else {
            sumVolSell += trade.v;
        }
    }

    total = sumVolSell + sumVolBuy;
    double netFlow = 0.0;
    if (total < 1e-10) {
        netFlow = 0.0;
    } else {
        netFlow = (sumVolBuy - sumVolSell) / total;
    }

    netFlowShort_ = alpha * netFlow + (1 - alpha) * netFlowShort_;
}

void TradeFlowImbalance::calculateBase() {
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                           .count();

    netFlowIntervalData_.clear();
    netFlowIntervalData_.resize(countIntervals, 0.0);
    sumVolBuy_.clear();
    sumVolBuy_.resize(countIntervals, 0.0);
    sumVolSell_.clear();
    sumVolSell_.resize(countIntervals, 0.0);

    for (const auto &trade : baseWindow_) {
        int index = (trade.T - (now - settings::baseTime.count())) / step;
        if (index >= 0 && index < countIntervals) {
            if (isBuy(trade)) {
                sumVolBuy_[index] += trade.v;
            } else {
                sumVolSell_[index] += trade.v;
            }
        }
    }

    // Теперь для каждого интервала считаем netFlow
    for (int i = 0; i < countIntervals; ++i) {
        double total = sumVolBuy_[i] + sumVolSell_[i];
        netFlowIntervalData_[i] = (total < 1e-10) ? 0 : (sumVolBuy_[i] - sumVolSell_[i]) / total;
    }

    mu_ = std::accumulate(netFlowIntervalData_.begin(), netFlowIntervalData_.end(), 0.0) / countIntervals;

    double sumDeltaSquare = 0.0;
    for (const auto &netFlow : netFlowIntervalData_) {
        sumDeltaSquare += pow(netFlow - mu_, 2);
    }
    sigma_ = sqrt(sumDeltaSquare / (countIntervals - 1));
}

void TradeFlowImbalance::checkSignal() {
    // if ((sigma_ < minSigma) || (sigma_ > maxSigma) || (shortWindow_.size() < minShortWinSize) ||
    //         (shortWindow_.size()) > maxShortWinSize) {
    //     signal_.store(Side_Unknown);
    //     zScore_ = 0.0;
    //     return;
    // }
    if ((sigma_ < minSigma) || (sigma_ > maxSigma)) {
        signal_.store(Side_Unknown);
        zScore_ = 0.0;
        return;
    }

    zScore_ = (netFlowShort_ - mu_) / sigma_;

    if (zScore_ >= settings::tradeFlowCountSigmaBuy) {
        signal_.store(Side_Buy);
    } else if (zScore_ <= settings::tradeFlowCountSigmaSell) {
        signal_.store(Side_Sell);
    } else {
        signal_.store(Side_Unknown);
    }
}

bool TradeFlowImbalance::isBuy(const SmallTradeData &trade) {
    if (trade.L == PublicTrade::TickDirection_PlusTick ||
            trade.L == PublicTrade::TickDirection_ZeroPlusTick) {
        return true;
    } else if (trade.L == PublicTrade::TickDirection_MinusTick ||
            trade.L == PublicTrade::TickDirection_ZeroMinusTick) {
        return false;
    }

    return false;
}

void TradeFlowImbalance::logData() {
    loger_.recordValue(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                               .count());
    loger_.recordValue(midPrice_.load());
    loger_.recordValue(netFlowShort_);
    loger_.recordValue(mu_);
    loger_.recordValue(sigma_);
    loger_.recordValue(zScore_);
    loger_.recordValue(signal_);
    loger_.recordValue(shortWindow_.size());
    loger_.recordValue(baseWindow_.size());
    loger_.recordValue(netFlowIntervalData_.size());
    loger_.endStr();
}
