#include "price_flow_divergence.h"
#include "settings.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string_view>
#include <vector>

namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

bool equalsIgnoreCase(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

//! Основной вариант: TP 2.0% / SL 0.5% / hold 24h, без trailing-stop.
//! Консервативный вариант: TP 1.5% / SL 0.5% / hold 24h.
//! Параметры и источники сигнала соответствуют Python-research.

PriceFlowDivergence::PriceFlowDivergence() :
buyVolAccum_(0.0),
sellVolAccum_(0.0),
currentIntervalStart_(0),
lastRolledIntervalStart_(0),
intervalActive_(false),
hasRolledInterval_(false),
priceSeries_(settings::pfdWindowSize + 1),
flowSeries_(settings::pfdWindowSize + 1),
sigPxSeries_(settings::pfdVolWindowSize),
rawSignalSeries_(settings::pfdConfirmIntervals),
priceVwap_(0.0),
tr_(0.0),
sigPx_(0.0),
stdFlow_(0.0),
pz_(0.0),
intz_(0.0),
comb_(0.0),
volRatio_(0.0),
volOk_(false),
signal_(Side_Unknown),
loger_("../log_files/price_flow_divergence.csv") {
    loger_.recordValue("ts");
    //! Имя колонки сохранено для совместимости decoder_PFD.py.
    //! Теперь здесь last-second VWAP, как в research, а не mid-price.
    loger_.recordValue("mid_price");
    loger_.recordValue("pz");
    loger_.recordValue("intz");
    loger_.recordValue("comb");
    loger_.recordValue("tr");
    loger_.recordValue("sig_px");
    loger_.recordValue("std_flow");
    loger_.recordValue("vol_ratio");
    loger_.recordValue("signal");
    loger_.endStr();
}

void PriceFlowDivergence::setPublicTrade(const PublicTrade &publicTrade) {
    if (publicTrade.ts == 0 || publicTrade.data.empty()) {
        return;
    }

    bool hasValidTrade = false;
    for (const auto &trade : publicTrade.data) {
        if (std::isfinite(trade.p) && std::isfinite(trade.v) && trade.p > 0.0 && trade.v >= 0.0) {
            hasValidTrade = true;
            break;
        }
    }
    if (!hasValidTrade) {
        return;
    }

    std::lock_guard lock(aggregationMutex_);

    const uint64_t intervalStep = static_cast<uint64_t>(settings::pfdIntervalStep.count());
    const uint64_t intervalStart = (publicTrade.ts / intervalStep) * intervalStep;

    if (intervalActive_ && intervalStart < currentIntervalStart_) {
        //! Поздняя сделка уже закрытого интервала. Research не пересчитывает
        //! ранее выданный live-сигнал, поэтому такой пакет пропускаем.
        return;
    }

    if (intervalActive_ && intervalStart > currentIntervalStart_) {
        rollInterval();
        intervalActive_ = false;
    }

    if (!intervalActive_) {
        if (hasRolledInterval_ && intervalStart <= lastRolledIntervalStart_) {
            return;
        }
        currentIntervalStart_ = intervalStart;
        secondBars_.clear();
        buyVolAccum_ = 0.0;
        sellVolAccum_ = 0.0;
        intervalActive_ = true;
    }

    const uint64_t secondStart = (publicTrade.ts / 1000) * 1000;
    auto &second = secondBars_[secondStart];
    for (const auto &trade : publicTrade.data) {
        if (!std::isfinite(trade.p) || !std::isfinite(trade.v) || trade.p <= 0.0 || trade.v < 0.0) {
            continue;
        }

        second.priceVolume += trade.p * trade.v;
        second.volume += trade.v;

        const Side side = getTradeSide(trade);
        if (side == Side_Buy) {
            buyVolAccum_ += trade.v;
        } else if (side == Side_Sell) {
            sellVolAccum_ += trade.v;
        }
    }
}

void PriceFlowDivergence::advanceTime(uint64_t now) {
    std::lock_guard lock(aggregationMutex_);
    if (!intervalActive_ || now == 0) {
        return;
    }

    const uint64_t intervalStep = static_cast<uint64_t>(settings::pfdIntervalStep.count());
    const uint64_t intervalStart = (now / intervalStep) * intervalStep;
    if (intervalStart > currentIntervalStart_) {
        rollInterval();
        intervalActive_ = false;
    }
}

bool PriceFlowDivergence::hasSignal() {
    return signal_.load() != Side_Unknown;
}

Side PriceFlowDivergence::getSignal() {
    return signal_.load();
}

uint64_t PriceFlowDivergence::getSignalVersion() const {
    return signalVersion_.load(std::memory_order_relaxed);
}

void PriceFlowDivergence::rollInterval() {
    lastRolledIntervalStart_ = currentIntervalStart_;
    hasRolledInterval_ = true;

    priceVwap_ = 0.0;
    if (!secondBars_.empty()) {
        const auto &lastSecond = secondBars_.rbegin()->second;
        if (lastSecond.volume > 0.0) {
            priceVwap_ = lastSecond.priceVolume / lastSecond.volume;
        }
    }

    const double totalVol = buyVolAccum_ + sellVolAccum_;
    const double flow = totalVol > 1e-12 ? (buyVolAccum_ - sellVolAccum_) / totalVol : 0.0;

    priceSeries_.push_back(priceVwap_);
    flowSeries_.push_back(flow);

    tr_ = 0.0;
    sigPx_ = 0.0;
    stdFlow_ = 0.0;
    pz_ = 0.0;
    intz_ = 0.0;
    comb_ = 0.0;

    //! До заполнения W+1 Python хранит sigPx=0 и включает его в rolling-медиану.
    if (priceSeries_.size() != priceSeries_.capacity()) {
        updateVolatilityRegime();
        checkSignal();
        return;
    }

    const size_t W = settings::pfdWindowSize;
    const double sqrtW = std::sqrt(static_cast<double>(W));

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

    //! Python: flow[i-W:i] - W предыдущих значений, текущий исключён.
    double meanFlow = 0.0;
    calcPreviousStats(flowSeries_, W, meanFlow, stdFlow_);

    updateVolatilityRegime();

    if (!std::isfinite(sigPx_) || !std::isfinite(stdFlow_) || sigPx_ < 1e-9 || stdFlow_ < 1e-9) {
        checkSignal();
        logData();
        return;
    }

    pz_ = tr_ / sigPx_;
    intz_ = meanFlow * sqrtW / stdFlow_;
    comb_ = pz_ - intz_;

    checkSignal();
    logData();
}

void PriceFlowDivergence::calcPreviousStats(const boost::circular_buffer<double> &buf, size_t count,
        double &mean, double &stdv) {
    mean = 0.0;
    stdv = 0.0;

    if (count < 1 || buf.size() < count + 1) {
        return;
    }

    const size_t begin = buf.size() - count - 1;
    const size_t end = buf.size() - 1;
    double sum = 0.0;
    for (size_t i = begin; i < end; ++i) {
        sum += buf[i];
    }
    mean = sum / count;

    double sumDeltaSquare = 0.0;
    for (size_t i = begin; i < end; ++i) {
        const double delta = buf[i] - mean;
        sumDeltaSquare += delta * delta;
    }
    stdv = std::sqrt(sumDeltaSquare / static_cast<double>(count - 1));
}

void PriceFlowDivergence::updateVolatilityRegime() {
    if (settings::pfdVolWindowSize < 2) {
        volOk_ = true;
        volRatio_ = 0.0;
        return;
    }

    //! pandas rolling в research видит нули до заполнения priceSeries W+1.
    sigPxSeries_.push_back(std::isfinite(sigPx_) ? sigPx_ : 0.0);
    volRatio_ = 0.0;

    if (sigPxSeries_.size() < sigPxSeries_.capacity()) {
        volOk_ = false;
        return;
    }

    std::vector<double> values(sigPxSeries_.begin(), sigPxSeries_.end());
    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    double median = values[middle];
    if (values.size() % 2 == 0) {
        const double lower = *std::max_element(values.begin(), values.begin() + middle);
        median = 0.5 * (lower + median);
    }

    if (median > 0.0) {
        volRatio_ = sigPx_ / median;
    }
    volOk_ = median > 0.0 && volRatio_ >= settings::pfdVolRatioMin &&
            volRatio_ <= settings::pfdVolRatioMax;
}

void PriceFlowDivergence::checkSignal() {
    Side rawSignal = Side_Unknown;
    if (std::isfinite(comb_)) {
        if (comb_ >= settings::pfdSigma) {
            rawSignal = Side_Buy;
        } else if (comb_ <= -settings::pfdSigma) {
            rawSignal = Side_Sell;
        }
    }

    Side confirmedSignal = rawSignal;

    if (settings::pfdConfirmIntervals > 0) {
        if (rawSignalSeries_.size() < rawSignalSeries_.capacity()) {
            confirmedSignal = Side_Unknown;
        } else {
            for (const Side previous : rawSignalSeries_) {
                if (previous != rawSignal) {
                    confirmedSignal = Side_Unknown;
                    break;
                }
            }
        }
        rawSignalSeries_.push_back(rawSignal);
    }

    if (!volOk_) {
        confirmedSignal = Side_Unknown;
    }

    signal_.store(confirmedSignal);
    signalVersion_.fetch_add(1, std::memory_order_relaxed);
}

Side PriceFlowDivergence::getTradeSide(const PublicTrade::Data &trade) const {
    const std::string_view side = trim(trade.S);
    if (equalsIgnoreCase(side, "Buy")) {
        return Side_Buy;
    }
    if (equalsIgnoreCase(side, "Sell")) {
        return Side_Sell;
    }

    if (trade.L == PublicTrade::TickDirection_PlusTick ||
            trade.L == PublicTrade::TickDirection_ZeroPlusTick) {
        return Side_Buy;
    }
    if (trade.L == PublicTrade::TickDirection_MinusTick ||
            trade.L == PublicTrade::TickDirection_ZeroMinusTick) {
        return Side_Sell;
    }
    return Side_Unknown;
}

void PriceFlowDivergence::logData() {
    loger_.recordValue(currentIntervalStart_);
    loger_.recordValue(priceVwap_);
    loger_.recordValue(pz_);
    loger_.recordValue(intz_);
    loger_.recordValue(comb_);
    loger_.recordValue(tr_);
    loger_.recordValue(sigPx_);
    loger_.recordValue(stdFlow_);
    loger_.recordValue(volRatio_);
    loger_.recordValue(static_cast<int>(signal_.load()));
    loger_.endStr();
}
