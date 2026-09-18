#include "price_flow_divergence.h"
#include "settings.h"

#include <chrono>
#include <cmath>

PriceFlowDivergence::PriceFlowDivergence() :
midPrice_(0.0),
buyVolAccum_(0.0),
sellVolAccum_(0.0),
lastCalcTime_(0),
priceSeries_(settings::pfdWindowSize + 1),
flowSeries_(settings::pfdWindowSize + 1),
tr_(0.0),
sigPx_(0.0),
stdFlow_(0.0),
pz_(0.0),
intz_(0.0),
comb_(0.0),
signal_(Side_Unknown),
loger_("../log_files/price_flow_divergence.csv") {
    loger_.recordValue("ts");
    loger_.recordValue("mid_price");
    loger_.recordValue("pz");
    loger_.recordValue("intz");
    loger_.recordValue("comb");
    loger_.recordValue("tr");
    loger_.recordValue("sig_px");
    loger_.recordValue("std_flow");
    loger_.recordValue("signal");
    loger_.endStr();
}

void PriceFlowDivergence::setPublicTrade(const PublicTrade &publicTrade) {
    for (const auto &trade : publicTrade.data) {
        if (isBuy(trade)) {
            buyVolAccum_ += trade.v;
        } else {
            sellVolAccum_ += trade.v;
        }
    }

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                           .count();
    maybeRollInterval(now);
}

void PriceFlowDivergence::setMidPrice(double price) {
    midPrice_.store(price);

    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                           .count();
    maybeRollInterval(now);
}

bool PriceFlowDivergence::hasSignal() {
    return signal_.load() != Side_Unknown;
}

Side PriceFlowDivergence::getSignal() {
    return signal_.load();
}

void PriceFlowDivergence::maybeRollInterval(uint64_t now) {
    if (lastCalcTime_ == 0) {
        lastCalcTime_ = now;
        return;
    }

    if (now - lastCalcTime_ < settings::pfdIntervalStep.count()) {
        return;
    }

    rollInterval();
    lastCalcTime_ = now;
}

void PriceFlowDivergence::rollInterval() {
    const double totalVol = buyVolAccum_ + sellVolAccum_;
    const double flow = (totalVol < 1e-12) ? 0.0 : (buyVolAccum_ - sellVolAccum_) / totalVol;

    priceSeries_.push_back(midPrice_.load());
    flowSeries_.push_back(flow);
    buyVolAccum_ = 0.0;
    sellVolAccum_ = 0.0;

    //! Пока не накопили окно W + текущий интервал, сигнал не считаем.
    if (priceSeries_.size() != priceSeries_.capacity()) {
        return;
    }

    const size_t W = settings::pfdWindowSize;
    const double sqrtW = std::sqrt(static_cast<double>(W));

    //! pz: моментум цены за окно W, нормированный на ожидаемую волатильность.
    tr_ = priceSeries_.back() / priceSeries_.front() - 1.0;

    double meanRet = 0.0;
    for (size_t i = priceSeries_.size() - W; i < priceSeries_.size(); ++i) {
        meanRet += priceSeries_[i] / priceSeries_[i - 1] - 1.0;
    }
    meanRet /= static_cast<double>(W);

    double sumDeltaSquare = 0.0;
    for (size_t i = priceSeries_.size() - W; i < priceSeries_.size(); ++i) {
        const double r = priceSeries_[i] / priceSeries_[i - 1] - 1.0;
        sumDeltaSquare += (r - meanRet) * (r - meanRet);
    }
    sigPx_ = std::sqrt(sumDeltaSquare / static_cast<double>(W - 1)) * sqrtW;

    //! intz: t-статистика накопленного потока за окно W.
    double meanFlow = 0.0;
    calcLastStats(flowSeries_, W, meanFlow, stdFlow_);

    if (sigPx_ < 1e-9 || stdFlow_ < 1e-9) {
        pz_ = 0.0;
        intz_ = 0.0;
        comb_ = 0.0;
        signal_.store(Side_Unknown);
        logData();
        return;
    }

    pz_ = tr_ / sigPx_;
    intz_ = meanFlow * sqrtW / stdFlow_;

    comb_ = pz_ - intz_;

    checkSignal();
    logData();
}

void PriceFlowDivergence::calcLastStats(const boost::circular_buffer<double> &buf, size_t count,
        double &mean, double &stdv) {
    mean = 0.0;
    stdv = 0.0;

    if (count < 1 || buf.size() < count) {
        return;
    }

    double sum = 0.0;
    for (size_t i = buf.size() - count; i < buf.size(); ++i) {
        sum += buf[i];
    }
    mean = sum / count;

    double sumDeltaSquare = 0.0;
    for (size_t i = buf.size() - count; i < buf.size(); ++i) {
        sumDeltaSquare += (buf[i] - mean) * (buf[i] - mean);
    }
    stdv = std::sqrt(sumDeltaSquare / (count - 1));
}

void PriceFlowDivergence::checkSignal() {
    if (comb_ >= settings::pfdSigma) {
        signal_.store(Side_Buy);
    } else if (comb_ <= -settings::pfdSigma) {
        signal_.store(Side_Sell);
    } else {
        signal_.store(Side_Unknown);
    }
}

bool PriceFlowDivergence::isBuy(const PublicTrade::Data &trade) {
    if (trade.L == PublicTrade::TickDirection_PlusTick ||
            trade.L == PublicTrade::TickDirection_ZeroPlusTick) {
        return true;
    }

    return false;
}

void PriceFlowDivergence::logData() {
    loger_.recordValue(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                               .count());
    loger_.recordValue(midPrice_.load());
    loger_.recordValue(pz_);
    loger_.recordValue(intz_);
    loger_.recordValue(comb_);
    loger_.recordValue(tr_);
    loger_.recordValue(sigPx_);
    loger_.recordValue(stdFlow_);
    loger_.recordValue(static_cast<int>(signal_.load()));
    loger_.endStr();
}
