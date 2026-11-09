#pragma once

#include "data_structures/common_data.h"
#include "data_structures/public_trade.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <boost/circular_buffer.hpp>
#include "loger/loger.h"

/**
 * @brief Дивергенция цены и потока: comb = pz - intz.
 * @details
 *  - intz = t-статистика потока за W интервалов, исключая текущий:
 *           intz = mean(flow[i-W:i]) * sqrt(W) / std(flow[i-W:i]);
 *  - pz   = нормированный моментум цены за окно W:
 *           pz = (curPrice / price(t-W) - 1) / (std(ret) * sqrt(W));
 *  - comb = pz - intz.
 *  Моментум-интерпретация:
 *   comb >= +pfdSigma -> Buy; comb <= -pfdSigma -> Sell.
 *
 *  Источники данных и агрегация соответствуют Python-research:
 *  - publicTrade.ts группирует сделки в 1-секундные бары;
 *  - цена интервала - VWAP последней доступной секунды;
 *  - интервалы - фиксированные UTC-бакеты pfdIntervalStep;
 *  - пустые интервалы не создаются;
 *  - sigPxSeries включает нулевые warmup-значения, как pandas rolling.
 *
 *  Дополнительно (параметры в settings.h):
 *  - pfdConfirmIntervals: сигнал подтверждается, если предыдущие N сырых
 *    сигналов были того же знака. Для N=1 это значит, что текущий и
 *    предыдущий интервалы дали одинаковый Buy/Sell.
 *  - pfdVolWindowSize / pfdVolRatioMin/Max: режимный фильтр волатильности.
 *    Торгуем только если текущий sigPx находится в заданном диапазоне
 *    относительно rolling-медианы sigPx за pfdVolWindowSize интервалов.
 *    Пока окно не заполнено, сигналы подавляются (warmup).
 */
class PriceFlowDivergence {
public:
    PriceFlowDivergence();

    void setPublicTrade(const PublicTrade &publicTrade);

    //! Закрывает активный UTC-интервал при переходе now на следующий.
    //! Цена из стакана в индикатор не передаётся и в расчёте не участвует.
    void advanceTime(uint64_t now);

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
    struct SecondAggregate {
        double priceVolume = 0.0;
        double volume = 0.0;
    };

    //! Накопленные объёмы за текущий интервал (для нормированного потока).
    double buyVolAccum_;
    double sellVolAccum_;

    //! Текущий фиксированный UTC-интервал и его 1-секундные бары.
    uint64_t currentIntervalStart_;
    uint64_t lastRolledIntervalStart_;
    bool intervalActive_;
    bool hasRolledInterval_;
    std::map<uint64_t, SecondAggregate> secondBars_;
    //! setPublicTrade и advanceTime могут вызываться из разных потоков.
    std::mutex aggregationMutex_;

    //! capacity = pfdWindowSize + 1: цены закрытия интервалов [t-W, t].
    boost::circular_buffer<double> priceSeries_;
    //! Нормированный поток ((buyVol - sellVol) / (buyVol + sellVol)) за интервал.
    boost::circular_buffer<double> flowSeries_;
    //! История sigPx для режимного фильтра волатильности, включая warmup-нули.
    boost::circular_buffer<double> sigPxSeries_;
    //! История сырых сигналов для подтверждения (N предыдущих интервалов).
    boost::circular_buffer<Side> rawSignalSeries_;

    double priceVwap_;
    double tr_;
    double sigPx_;
    double stdFlow_;

    double pz_;
    double intz_;
    double comb_;
    double volRatio_;

    //! Разрешена ли торговля текущим режимом волатильности.
    bool volOk_;

    std::atomic<Side> signal_;
    std::atomic<uint64_t> signalVersion_ {0};

    Logger loger_;

    void rollInterval();

    //! W значений, заканчивающихся предыдущим интервалом (текущий исключён).
    void calcPreviousStats(const boost::circular_buffer<double> &buf, size_t count, double &mean,
            double &stdv);

    void updateVolatilityRegime();
    void checkSignal();

    Side getTradeSide(const PublicTrade::Data &trade) const;

    void logData();
};
