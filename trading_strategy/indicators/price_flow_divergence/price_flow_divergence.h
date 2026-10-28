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
 * @brief Дивергенция цены и потока: comb = pz - intz.
 * @details
 *  - intz = t-статистика накопленного потока за окно W:
 *           intz = mean(flow) * sqrt(W) / std(flow);
 *  - pz   = нормированный моментум цены за окно W:
 *           pz = (curPrice / price(t-W) - 1) / (std(ret) * sqrt(W));
 *  - comb = pz - intz.
 *  Моментум-интерпретация:
 *   comb >= +pfdSigma -> Buy; comb <= -pfdSigma -> Sell.
 *
 *  Дополнительно (параметры в settings.h):
 *  - pfdConfirmIntervals: сигнал подтверждается, если предыдущие N сырых
 *    сигналов были того же знака. Для N=1 это значит, что текущий и
 *    предыдущий интервалы дали одинаковый Buy/Sell.
 *  - pfdVolWindowSize / pfdVolRatioMin/Max: режимный фильтр волатильности.
 *    Торгуем только если текущий sigPx находится в заданном диапазоне
 *    относительно rolling-медианы sigPx за pfdVolWindowSize интервалов.
 *    Пока окно не заполнено, сигналы подавляются (warmup).
 *
 *  ВАЖНО: offline-исследование использовало price_source=vwap
 *  (VWAP последней секунды интервала). В C++ используется mid-price из
 *  стакана (setMidPrice), поэтому live-сигналы могут отличаться.
 */
class PriceFlowDivergence {
public:
    PriceFlowDivergence();

    void setPublicTrade(const PublicTrade &publicTrade);
    void setMidPrice(double price);

    bool hasSignal();

    Side getSignal();

    /**
     * @brief Версия сигнала. Увеличивается на каждом пересчёте интервала,
     * даже если значение сигнала не изменилось.
     * @details Нужна стратегии, чтобы отличать новый сигнал от повторного
     * чтения того же значения в цикле onMarketUpdate().
     */
    uint64_t getSignalVersion() const;

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
    //! История sigPx для режимного фильтра волатильности.
    boost::circular_buffer<double> sigPxSeries_;
    //! История сырых сигналов для подтверждения (N предыдущих интервалов).
    boost::circular_buffer<Side> rawSignalSeries_;

    double tr_;
    double sigPx_;
    double stdFlow_;

    double pz_;
    double intz_;
    double comb_;

    //! Разрешена ли торговля текущим режимом волатильности.
    bool volOk_;

    std::atomic<Side> signal_;
    std::atomic<uint64_t> signalVersion_ {0};

    Logger loger_;

    void maybeRollInterval(uint64_t now);
    void rollInterval();

    void calcLastStats(const boost::circular_buffer<double> &buf, size_t count, double &mean,
            double &stdv);

    void updateVolatilityRegime();
    void checkSignal();

    bool isBuy(const PublicTrade::Data &trade);

    void logData();
};