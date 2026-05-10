#pragma once

#include "base_trading_strategy.h"

#include <atomic>
#include <boost/circular_buffer.hpp>

// struct TradeRecord {
//     uint64_t timestamp;  // Время сделки (у тебя уже есть поле T)
//     double price;        // Цена (поле p)
//     double volume;       // Объем в базовой валюте (поле v)
//     PublicTrade::TickDirection direction; // Направление (поле L)
// };

class ImbalanceAndLarge : public BaseTradingStrategy {
public:
    ImbalanceAndLarge(ConnectionManager* connManager);

    virtual void setOrderbook(const OrderBook &orderbook) override;
    virtual void setPublicTradeData(PublicTrade::VectorData &&publicTradeData) override;

protected:
    void onMarketUpdate() override;


private:
    void calcDisbalance(const OrderBook &orderbook);

    std::atomic<float> disbalance_;
    std::atomic<bool> orderbookIsUpdate_;
    std::atomic<bool> publicTradeIsUpdate_;

    /**
     * @brief Кольцевой буфер для хранения средней цены.
     * @details Работает так: если в конец добавляется новое значение и size < capacity,
     * то новый элемент просто добавится. Если size = capacity, то первый (самый старый) элемент
     * удалится, свободное место переставится в конец и запишется новое значение.
     * Сложность вставки в конец - O(1).
     */
    boost::circular_buffer<double> midPrices_;
    boost::circular_buffer<std::vector<PublicTrade::Data> > publicTrades_;
};
