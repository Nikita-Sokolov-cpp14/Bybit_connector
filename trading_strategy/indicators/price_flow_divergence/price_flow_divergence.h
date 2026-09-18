#pragma once

#include "data_structures/orderbook.h"
#include "data_structures/public_trade.h"
#include "data_structures/order_request.h"
#include "data_structures/order.h"
#include "data_structures/position.h"
#include "trade_manager/trade_manager.h"

#include <atomic>
#include <cstdint>
#include <boost/circular_buffer.hpp>
#include "loger/loger.h"

/**
 * @brief Дивергенция цены и потока: comb = pz - intz (формула comb_W360 из decoder_LTFI.py).
 * @details
 *  - intz = t-статистика накопленного потока за окно W:
 *           intz = mean(flow) * sqrt(W) / std(flow);
 *  - pz   = нормированный моментум цены за окно W:
 *           pz = (curPrice / price(t-W) - 1) / (std(ret) * sqrt(W));
 *  - comb = pz - intz.
 *  Эмпирика на логах (decoder_LTFI.py): высокий comb (цена обгоняет поток)
 *  предсказывает продолжение движения вверх, низкий comb — движение вниз.
 *  Поэтому моментум-интерпретация (обратная старой версии "поток ведёт, цена догоняет"):
 *   comb >= +pfdSigma -> Buy; comb <= -pfdSigma -> Sell.
 */
class PriceFlowDivergence {
public:
    PriceFlowDivergence();

    void setPublicTrade(const PublicTrade &publicTrade);
    void setMidPrice(double price);

    bool hasSignal();

    Side getSignal();

private:
    //! Последняя известная мид-цена (обновляется из стакана).
    std::atomic<double> midPrice_;
    //! Накопленные объёмы за текущий интервал (для нормированного потока).
    double buyVolAccum_;
    double sellVolAccum_;
    //! Время последнего пересчёта интервала, мс.
    uint64_t lastCalcTime_;

    //! capacity = pfdWindowSize + 1: цены закрытия интервалов [t-W, t].
    boost::circular_buffer<double> priceSeries_;
    //! Нормированный поток ((buyVol - sellVol) / (buyVol + sellVol)) за интервал.
    boost::circular_buffer<double> flowSeries_;

    double tr_;
    double sigPx_;
    double stdFlow_;

    double pz_;
    double intz_;
    double comb_;

    std::atomic<Side> signal_;

    Logger loger_;

    void maybeRollInterval(uint64_t now);
    void rollInterval();

    void calcLastStats(const boost::circular_buffer<double> &buf, size_t count, double &mean,
            double &stdv);

    void checkSignal();

    bool isBuy(const PublicTrade::Data &trade);

    void logData();
};