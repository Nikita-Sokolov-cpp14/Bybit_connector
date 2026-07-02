#pragma once

#include "base_trading_strategy.h"

#include <atomic>
#include <boost/circular_buffer.hpp>
#include <optional>
#include <numeric>
#include "indicators/imbalance/imbalance_indicator.h"
#include "indicators/trade_flow/trade_flow.h"

// struct TradeRecord {
//     uint64_t timestamp;  // Время сделки (у тебя уже есть поле T)
//     double price;        // Цена (поле p)
//     double volume;       // Объем в базовой валюте (поле v)
//     PublicTrade::TickDirection direction; // Направление (поле L)
// };

class ImbalanceAndLarge : public BaseTradingStrategy {
public:
    ImbalanceAndLarge();

    void setOrderbook(const OrderBook &orderbook) override;
    void setPublicTradeData(const PublicTrade &publicTrade) override;
    void setOrder(const OrderHFT &order) override;
    void setPosition(const PositionHFT &position) override;

protected:
    void onMarketUpdate() override;

private:
    std::atomic<bool> publicTradeIsUpdate_;
    std::atomic<double> currentBestAskPrice_;
    std::atomic<double> currentBestBidPrice_;
    std::atomic<double> currentMidPrice_;

    /**
     * @brief Кольцевой буфер для хранения средней цены.
     * @details Работает так: если в конец добавляется новое значение и size < capacity,
     * то новый элемент просто добавится. Если size = capacity, то первый (самый старый) элемент
     * удалится, свободное место переставится в конец и запишется новое значение.
     * Сложность вставки в конец - O(1).
     */
    boost::circular_buffer<double> midPrices_;
    boost::circular_buffer<std::vector<PublicTrade::Data> > publicTrades_;
    std::optional<Side> signalTrade_;
    std::optional<Side> signalTotal_;

    ImbalanceIndicator imbalanceIndicator_;
    TradeFlowIndicator tradeFlowIndicator_;

    void checkSignalTrades();

    void checkSignalTotal();
};
