#include "order_book_imbalance.h"

#include <chrono>
#include <cmath>
#include "settings.h"

namespace {

const size_t nEff = 250;
const double alpha = 2.0 / (nEff + 1.0);

const size_t nEffFast = 90;
const double alphaFast = 2.0 / (nEffFast + 1.0);

const size_t nStd = 3 * settings::obiAgrWindowSizePrev;

} // namespace

OrderBookImbalance::OrderBookImbalance() :
aggObiStorage_(settings::averrageAgrCount),
disbalanceAverage_(0.0),
signal_(Side_Unknown),
indexAgregateData_(0),
emaObi_(0.0),
emaObiPrev_(0.0),
emaRecent_(0.0),
emaRecentT_1_(0.0),
smaPrev_(0.0),
sigma_(0.0),
dwObi_(0.0),
midPrice_(0.0),
loger_("../log_files/imbalance.csv") {
    loger_.recordValue("ts");
    loger_.recordValue("mid_price");
    loger_.recordValue("DW_OBI");
    loger_.recordValue("ema_OBI");
    loger_.recordValue("agg_OBI");
    loger_.recordValue("OBI_recent");
    loger_.recordValue("OBI_prev");
    loger_.recordValue("sigma");
    loger_.recordValue("z_score");
    loger_.recordValue("signal");
    loger_.endStr();
}

void OrderBookImbalance::setOrderbook(const OrderBook &orderbook) {
    dwObi_ = calcDisbalance(orderbook);
    emaObi_ = dwObi_ * alpha + emaObiPrev_ * (1.0 - alpha);
    emaObiPrev_ = emaObi_;

    if (indexAgregateData_ == (settings::agregateCountObi - 1)) {
        aggObiStorage_.push_back(emaObi_);
        indexAgregateData_ = 0;
    } else {
        ++indexAgregateData_;
        return;
    }

    //! Если не накопили данные, то и сигнал не рассматриваем
    if (aggObiStorage_.size() < nStd) {
        signal_.store(Side_Unknown);
        return;
    }

    emaRecent_ = aggObiStorage_.back() * alphaFast + emaRecentT_1_ * (1.0 - alphaFast);
    emaRecentT_1_ = emaRecent_;

    auto beginIt = aggObiStorage_.end() - settings::obiAgrWindowSizePrev;
    auto endIt = aggObiStorage_.end();
    smaPrev_ = std::accumulate(beginIt, endIt, 0.0) / settings::obiAgrWindowSizePrev;

    midPrice_ = (orderbook.asks.begin()->first + orderbook.bids.begin()->first) / 2.0;

    getSKO();
    checkSignal();
    logData();
}

bool OrderBookImbalance::hasSignal() {
    return signal_.load() != Side_Unknown;
}

Side OrderBookImbalance::getSignal() {
    return signal_.load();
}

double OrderBookImbalance::calcDisbalance(const OrderBook &orderbook) {
    double sumVolAsks = 0;
    double sumVolBids = 0;

    const size_t depthAsks = std::min(settings::disbalanceDepthCalc, orderbook.asks.size());
    const size_t depthBids = std::min(settings::disbalanceDepthCalc, orderbook.bids.size());

    const double bestAsk = orderbook.asks.begin()->first;
    const double bestBid = orderbook.bids.begin()->first;

    for (auto it = orderbook.asks.begin(); it < orderbook.asks.begin() + depthAsks; ++it) {
        sumVolAsks += it->second;
    }

    for (auto it = orderbook.bids.begin(); it < orderbook.bids.begin() + depthBids; ++it) {
        sumVolBids += it->second;
    }

    if ((sumVolBids + sumVolAsks) < 1e-10)
        return 0.0;

    return (sumVolBids - sumVolAsks) / (sumVolBids + sumVolAsks);
}

void OrderBookImbalance::checkSignal() {
    if (sigma_ < 1e-10) {
        signal_.store(Side_Unknown);
        return;
    }

    double zNormal = (emaRecent_ - smaPrev_) / sigma_;

    if (zNormal >= settings::countSigmaBuy) {
        signal_.store(Side_Buy);
    } else if (zNormal <= settings::countSigmaSell) {
        signal_.store(Side_Sell);
    } else {
        signal_.store(Side_Unknown);
    }
}

void OrderBookImbalance::getSKO() {
    if (aggObiStorage_.size() < nStd) {
        return;
    }

    double sumDeltaSquare = 0.0;

    double sumObi = 0.0;
    for (size_t i = aggObiStorage_.size() - nStd; i < aggObiStorage_.size(); ++i) {
        sumObi += aggObiStorage_[i];
    }
    disbalanceAverage_ = sumObi / (nStd);

    for (size_t i = aggObiStorage_.size() - nStd; i < aggObiStorage_.size(); ++i) {
        sumDeltaSquare += pow(aggObiStorage_[i] - disbalanceAverage_, 2);
    }
    sigma_ = sqrt(sumDeltaSquare / (nStd - 1));
}

void OrderBookImbalance::logData() {
    // Базовые данные
    loger_.recordValue(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                               .count()); // ts
    loger_.recordValue(midPrice_); // mid_price
    loger_.recordValue(dwObi_); // DW_OBI

    // Компоненты индикатора
    loger_.recordValue(emaObi_); // ema_obi
    loger_.recordValue(aggObiStorage_.back()); // agg_obi
    loger_.recordValue(emaRecent_); // obi_recent (быстрый)
    loger_.recordValue(smaPrev_); // obi_prev (медленный)

    // Статистика
    loger_.recordValue(sigma_); // std_dev
    loger_.recordValue((emaRecent_ - smaPrev_) / sigma_); // z_score

    // Сигнал
    loger_.recordValue(static_cast<int>(signal_.load())); // signal (0=Unknown, 1=Buy, -1=Sell)

    loger_.endStr();
}
