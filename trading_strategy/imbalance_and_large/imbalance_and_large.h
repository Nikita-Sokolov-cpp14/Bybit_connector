#pragma once

#include "base_trading_strategy.h"

#include <atomic>
#include <boost/circular_buffer.hpp>
#include <optional>
#include <numeric>

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
    void setPublicTradeData(PublicTrade::VectorData &&publicTradeData) override;
    void setOrder(const OrderHFT &order) override;
    void setPosition(const PositionHFT &position) override;

protected:
    void onMarketUpdate() override;

private:
    TradeManager::OrderMainData orderMainData_;
    std::mutex orderMt_;

    std::atomic<float> disbalanceAverage_;
    std::atomic<bool> orderbookIsUpdate_;
    std::atomic<bool> publicTradeIsUpdate_;
    std::atomic<bool> needCheckCurPrice_;
    std::atomic<bool> orderIsUpdate_;
    std::atomic<double> currentBestAskPrice_;
    std::atomic<double> currentBestBidPrice_;
    std::atomic<double> currentMidPrice_;

    double calcDisbalance(const OrderBook &orderbook);

    /**
     * @brief Кольцевой буфер для хранения средней цены.
     * @details Работает так: если в конец добавляется новое значение и size < capacity,
     * то новый элемент просто добавится. Если size = capacity, то первый (самый старый) элемент
     * удалится, свободное место переставится в конец и запишется новое значение.
     * Сложность вставки в конец - O(1).
     */
    boost::circular_buffer<double> midPrices_;
    boost::circular_buffer<double> midDisbalance_;
    boost::circular_buffer<std::vector<PublicTrade::Data> > publicTrades_;
    std::optional<Side> signalDisbalance_;
    std::optional<Side> signalInverseDisbalance_;
    std::optional<Side> signalTrade_;
    std::optional<Side> signalTotal_;
    std::atomic<size_t> countInverseSignal_;
    std::atomic<size_t> countSignal_;

    void checkSignalTrades();
    void checkSignalDisbalance();
};
