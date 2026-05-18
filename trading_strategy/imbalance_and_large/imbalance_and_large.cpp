#include "imbalance_and_large.h"

#include <iostream>

#include "settings.h"
#include "data_structures/public_trade.h"

ImbalanceAndLarge::ImbalanceAndLarge() :
disbalanceAverage_(0),
orderbookIsUpdate_(false),
publicTradeIsUpdate_(false),
midPrices_(settings::historyMidPriceSize),
midDisbalance_(settings::averrageDisbalanceCount),
publicTrades_(settings::historyPublicTradeSize),
currentBestAskPrice_(0.0),
currentBestBidPrice_(0.0),
currentMidPrice_(0.0),
signalDisbalance_(std::nullopt),
signalLargeDisbalance_(std::nullopt),
signalTrade_(std::nullopt),
signalTotal_(std::nullopt),
countInverseSignal_(0),
countSignal_(0) {
}

void ImbalanceAndLarge::setOrderbook(const OrderBook &orderbook) {
    midDisbalance_.push_back(calcDisbalance(orderbook));
    disbalanceAverage_.store(std::accumulate(midDisbalance_.begin(), midDisbalance_.end(), 0.0) /
            midDisbalance_.size());

    currentMidPrice_.store((orderbook.asks.begin()->first + orderbook.bids.begin()->first) / 2.0);
    currentBestAskPrice_.store(orderbook.asks.begin()->first);
    currentBestBidPrice_.store(orderbook.bids.begin()->first);
    midPrices_.push_back(currentMidPrice_.load());
    orderbookIsUpdate_.store(true);
    newData_.store(true);

    tradeManager_.checkCurrentPrice(currentMidPrice_.load());

    dataCV_.notify_all();

    // Проверить выход по таймауту
}

void ImbalanceAndLarge::setPublicTradeData(PublicTrade::VectorData &&publicTradeData) {
    //! TODO: После этой опреации у publicTrade больше нет данных.
    //! TODO: Нужно успеть обработать обновление сделок до прихода новых.
    // Предполагается, что onMarketUpdate работает меньше 1 мс. За это время новая
    // информация не поступит.
    publicTrades_.push_back(std::move(publicTradeData));
    publicTradeIsUpdate_.store(true);
    newData_.store(true);
    dataCV_.notify_all();
}

double ImbalanceAndLarge::calcDisbalance(const OrderBook &orderbook) {
    double sumVolAsks = 0;
    double sumVolBids = 0;

    const size_t depthAsks = std::min(settings::disbalanceDepthCalc, orderbook.asks.size());
    const size_t depthBids = std::min(settings::disbalanceDepthCalc, orderbook.bids.size());

    for (auto it = orderbook.asks.begin(); it < orderbook.asks.begin() + depthAsks; ++it) {
        sumVolAsks += it->second;
    }

    for (auto it = orderbook.bids.begin(); it < orderbook.bids.begin() + depthBids; ++it) {
        sumVolBids += it->second;
    }

    if (sumVolAsks == 0) {
        return 1.0;
    }

    return sumVolBids / sumVolAsks;
}

void ImbalanceAndLarge::onMarketUpdate() {
    //! TODO: Дисбаланс - основной сигнал
    if (orderbookIsUpdate_) {
        checkSignalDisbalance();
        orderbookIsUpdate_ = false;
    }

    //! TODO: Крупная сделка - по большей части просто подтверждение.
    if (publicTradeIsUpdate_) {
        checkSignalTrades();
        publicTradeIsUpdate_ = false;
    }

    if (signalDisbalance_.has_value() && signalTrade_.has_value() &&
            countSignal_.load() >= settings::countSignal) {
        if (signalDisbalance_.value() == Side_Buy && signalTrade_.value() == Side_Buy) {
            // std::cout << "ImbalanceAndLarge: total buy" << std::endl;
            tradeManager_.makeTrade(currentBestBidPrice_, Side_Buy);
            signalTotal_ = Side_Buy;
        } else if (signalDisbalance_.value() == Side_Sell && signalTrade_.value() == Side_Sell) {
            // std::cout << "ImbalanceAndLarge: total sell" << std::endl;
            tradeManager_.makeTrade(currentBestAskPrice_, Side_Sell);
            signalTotal_ = Side_Sell;
        } else {
            countSignal_.store(0);
            signalTotal_ = std::nullopt;
        }
    } else {
        signalTotal_ = std::nullopt;
    }

    //! TODO: Если встретился противоположный сигнал.
    // Подумать - закрывать только по противоположному дисбалансу или по полному противоположному сигналу?
    // if (signalDisbalance_.has_value()) {
    //     tradeManager_.checkInverseSignal(signalDisbalance_.value());
    // }
    //! TODO: Здесь закрытие по полному противоположному сигналу.
    //! TODO: Пока противоположный сигнал никак не учитываем
    if (signalLargeDisbalance_.has_value() &&
            countInverseSignal_.load() >= settings::countInverseSignal) {
        tradeManager_.checkInverseSignal(signalLargeDisbalance_.value());
    }
}

void ImbalanceAndLarge::checkSignalTrades() {
    //! TODO: Нужно внимательно проследить, что новые данные о сделках не добавятся.
    size_t countPlusTick = 0;
    size_t countminusTick = 0;
    for (const auto &trade : publicTrades_.back()) {
        if (trade.L == PublicTrade::TickDirection_PlusTick) {
            countPlusTick++;
        }
        if (trade.L == PublicTrade::TickDirection_MinusTick) {
            countminusTick++;
        }
    }

    //! TODO: Это строгий сигнал. Можно ослабить, если будет очень мало сделок.
    if (countPlusTick > 0 && countminusTick == 0) {
        signalTrade_ = Side_Buy;
    } else if (countminusTick > 0 && countPlusTick == 0) {
        signalTrade_ = Side_Sell;
    } else {
        signalTrade_ = std::nullopt;
    }
}

void ImbalanceAndLarge::checkSignalDisbalance() {
    if (disbalanceAverage_ >= settings::buyDisbalance) {
        if (signalDisbalance_ == Side_Buy) {
            countSignal_++;
        } else {
            countSignal_.store(1);
        }
        signalDisbalance_ = Side_Buy;
    } else if (disbalanceAverage_ <= settings::sellDisbalance) {
        if (signalDisbalance_ == Side_Sell) {
            countSignal_++;
        } else {
            countSignal_.store(1);
        }
        signalDisbalance_ = Side_Sell;
    } else {
        countSignal_.store(0);
        signalDisbalance_ = std::nullopt;
    }

    if (disbalanceAverage_ >= settings::inverseBuyDisbalance) {
        if (signalLargeDisbalance_ == Side_Buy) {
            countInverseSignal_++; // продолжаем ту же серию Buy
        } else {
            countInverseSignal_.store(1); // новая серия Buy
        }
        signalLargeDisbalance_ = Side_Buy;
    } else if (disbalanceAverage_ <= settings::inverseSellDisbalance) {
        if (signalLargeDisbalance_ == Side_Sell) {
            countInverseSignal_++; // продолжаем ту же серию Sell
        } else {
            countInverseSignal_.store(1); // новая серия Sell
        }
        signalLargeDisbalance_ = Side_Sell;
    } else {
        signalLargeDisbalance_ = std::nullopt;
        countInverseSignal_.store(0);
    }
}
