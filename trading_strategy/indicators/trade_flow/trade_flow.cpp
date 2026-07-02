#include "trade_flow.h"
#include "settings.h"

#include <chrono>
#include <iostream>
#include <cmath>

namespace {

// шаг выборки, мс
const size_t step = 100;

const size_t minCountNetFlow = 20;

const size_t maxCountEmptyIntervals = 10;

} // namespace

TradeFlowIndicator::TradeFlowIndicator() :
signal_(Side_Unknown),
netFlowShort_(0.0),
mu_(0.0),
sigma_(0.0),
sumVolBuy_(0.0),
sumVolSell_(0.0),
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

void TradeFlowIndicator::setPublicTrade(const PublicTrade &publicTrade) {
    for (const auto &trade : publicTrade.data) {
        SmallTradeData tradeData;
        tradeData.BT = trade.BT;
        tradeData.L = trade.L;
        tradeData.p = trade.p;
        tradeData.RPI = trade.RPI;
        tradeData.seq = trade.seq;
        tradeData.T = trade.T;
        tradeData.v = trade.v;
        // std::cout << "TradeFlowIndicator::setPublicTrade " << tradeData.T << " "
        //           << std::chrono::duration_cast<std::chrono::milliseconds>(
        //                      std::chrono::system_clock::now().time_since_epoch()).count()
        //           << std::endl;

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

bool TradeFlowIndicator::hasSignal() {
    if (signal_.load() != Side_Unknown) {
        return true;
    }

    return false;
}

Side TradeFlowIndicator::getSignal() {
    return signal_.load();
}

void TradeFlowIndicator::setMidPrice(double price) {
    midPrice_.store(price);
}

void TradeFlowIndicator::checkActualityTime() {
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

void TradeFlowIndicator::calculateShort() {
    sumVolBuy_ = 0.0;
    sumVolSell_ = 0.0;
    double total = 0.0;

    for (const auto &trade : shortWindow_) {
        checkDirection(trade);
    }

    total = sumVolSell_ + sumVolBuy_;
    if (total < 1e-10) {
        netFlowShort_ = 0.0;
        return;
    }

    netFlowShort_ = (sumVolBuy_ - sumVolSell_) / total;
}

void TradeFlowIndicator::calculateBase() {
    double timeStartInterval = baseWindow_.begin()->T;
    double netFlowInterval = 0.0;
    double total = 0.0;

    sumVolBuy_ = 0.0;
    sumVolSell_ = 0.0;
    netFlowIntervalData_.clear();

    for (const auto &trade : baseWindow_) {
        if ((trade.T - timeStartInterval) > step) {
            // 1. Сохраняем текущий интервал
            total = sumVolSell_ + sumVolBuy_;
            netFlowInterval = (total < 1e-10) ? 0 : (sumVolBuy_ - sumVolSell_) / total;
            netFlowIntervalData_.push_back(netFlowInterval);

            // 2. Вставляем пустые интервалы
            int countEmptyInterval = (trade.T - timeStartInterval) / step;
            int emptyIntervalsToInsert = countEmptyInterval - 1;
            if (emptyIntervalsToInsert > 0) {
                insertEmptyIntervals(emptyIntervalsToInsert);
            }

            // 3. Начинаем новый интервал
            timeStartInterval += step * countEmptyInterval;
            sumVolBuy_ = 0.0;
            sumVolSell_ = 0.0;
        }
        checkDirection(trade);
    }

    if (netFlowIntervalData_.size() < minCountNetFlow) {
        mu_ = 0.0;
        sigma_ = 0.0;
        return;
    }

    mu_ = std::accumulate(netFlowIntervalData_.begin(), netFlowIntervalData_.end(), 0.0) /
            netFlowIntervalData_.size();

    double sumDeltaSquare = 0.0;
    for (const auto &netFlow : netFlowIntervalData_) {
        sumDeltaSquare += pow(netFlow - mu_, 2);
    }
    sigma_ = sqrt(sumDeltaSquare / (netFlowIntervalData_.size() - 1));
}

void TradeFlowIndicator::checkSignal() {
    if (sigma_ < 1e-10) {
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

void TradeFlowIndicator::checkDirection(const SmallTradeData &trade) {
    switch (trade.L) {
        case PublicTrade::TickDirection_PlusTick:
            sumVolBuy_ += trade.v;
            break;
        case PublicTrade::TickDirection_ZeroPlusTick:
            sumVolBuy_ += trade.v;
            break;
        case PublicTrade::TickDirection_MinusTick:
            sumVolSell_ += trade.v;
            break;
        case PublicTrade::TickDirection_ZeroMinusTick:
            sumVolSell_ += trade.v;
            break;
        default:
            break;
    }
}

void TradeFlowIndicator::insertEmptyIntervals(int countEmptyInterval) {
    if (countEmptyInterval > maxCountEmptyIntervals) {
        countEmptyInterval = maxCountEmptyIntervals;
    }

    for (size_t i = 0; i < countEmptyInterval; ++i) {
        netFlowIntervalData_.push_back(0.0);
    }
}

void TradeFlowIndicator::logData() {
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
