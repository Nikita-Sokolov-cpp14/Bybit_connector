#pragma once

#include "base_trading_strategy.h"

#include <atomic>
#include <boost/circular_buffer.hpp>
#include <deque>
#include <mutex>
#include <optional>
#include <numeric>
#include <vector>
#include "indicators/order_book_imbalance/order_book_imbalance.h"
#include "indicators/trade_flow_imbalance/trade_flow_imbalance.h"
#include "indicators/price_flow_divergence/price_flow_divergence.h"
#include "loger/loger.h"

// struct TradeRecord {
//     uint64_t timestamp;  // Время сделки (у тебя уже есть поле T)
//     double price;        // Цена (поле p)
//     double volume;       // Объем в базовой валюте (поле v)
//     PublicTrade::TickDirection direction; // Направление (поле L)
// };

class ImbalanceAndLarge : public BaseTradingStrategy {
public:
    ImbalanceAndLarge();
    ~ImbalanceAndLarge() override;

    void setOrderbook(const OrderBook &orderbook) override;
    void setPublicTradeData(const PublicTrade &publicTrade) override;
    void setOrder(const OrderHFT &order) override;
    void setPosition(const PositionHFT &position) override;
    void setOrderRequestResult(const OrderRequestResult &result) override;

protected:
    void onMarketUpdate() override;

private:
    //! Обновления ордеров приходят из IO-потока. Копим их и обрабатываем
    //! в потоке стратегии, чтобы не было гонок с checkCurrentPrice()/makeTrade().
    std::mutex orderQueueMutex_;
    std::vector<OrderEvent> orderQueue_;
    std::vector<OrderRequestResult> orderRequestResults_;

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
    boost::circular_buffer<std::vector<PublicTrade::Data> > publicTrades_;
    // std::optional<Side> signalTfi_;
    // std::optional<Side> signalObi_;
    std::optional<Side> signalPfd_;
    std::optional<Side> signalTotal_;
    uint64_t lastSignalVersion_ = 0;

    PriceFlowDivergence priceFlowDivergence_;
    // OrderBookImbalance orderBookImbalance_;
    // TradeFlowImbalance tradeFlowImbalance_;

    Logger loger_;

    void checkSignalTotal();
    void logData();
    void drainOrderUpdates();
    void drainOrderRequestResults();
};
