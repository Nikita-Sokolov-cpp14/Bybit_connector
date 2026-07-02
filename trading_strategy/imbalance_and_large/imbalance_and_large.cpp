#include "imbalance_and_large.h"

#include <iostream>
#include <algorithm>

#include "settings.h"
#include "data_structures/public_trade.h"

ImbalanceAndLarge::ImbalanceAndLarge() :
publicTradeIsUpdate_(false),
midPrices_(settings::historyMidPriceSize),
publicTrades_(settings::historyPublicTradeSize),
currentBestAskPrice_(0.0),
currentBestBidPrice_(0.0),
currentMidPrice_(0.0),
signalTrade_(std::nullopt),
signalTotal_(std::nullopt),
imbalanceIndicator_(),
tradeFlowIndicator_() {
}

void ImbalanceAndLarge::setOrderbook(const OrderBook &orderbook) {
    //! TODO:
    // imbalanceIndicator_.setOrderbook(orderbook);

    currentMidPrice_.store((orderbook.asks.begin()->first + orderbook.bids.begin()->first) / 2.0);
    currentBestAskPrice_.store(orderbook.asks.begin()->first);
    currentBestBidPrice_.store(orderbook.bids.begin()->first);
    midPrices_.push_back(currentMidPrice_.load());
    tradeFlowIndicator_.setMidPrice(currentMidPrice_.load());

    tradeManager_.checkCurrentPrice(currentMidPrice_.load());

    newData_.store(true);
    dataCV_.notify_all();
}

void ImbalanceAndLarge::setPublicTradeData(const PublicTrade &publicTrade) {
    tradeFlowIndicator_.setPublicTrade(publicTrade);
    //! TODO: После этой опреации у publicTrade больше нет данных.
    //! TODO: Нужно успеть обработать обновление сделок до прихода новых.
    // Предполагается, что onMarketUpdate работает меньше 1 мс. За это время новая
    // информация не поступит.
    // publicTrades_.push_back(std::move(publicTradeData));
    // publicTradeIsUpdate_.store(true);
    // newData_.store(true);
    // dataCV_.notify_all();
}

void ImbalanceAndLarge::setOrder(const OrderHFT &order) {
    std::cout << "ImbalanceAndLarge::setOrder" << std::endl;
    std::cout << "id " << order.id << std::endl;
    std::cout << "orderLinkId " << order.orderLinkId << std::endl;
    std::cout << "side " << order.side << std::endl;
    std::cout << "orderStatus " << order.orderStatus << std::endl;
    std::cout << "orderType " << order.orderType << std::endl;
    std::cout << "price " << order.price << std::endl;
    std::cout << "qty " << order.qty << std::endl;
}

void ImbalanceAndLarge::setPosition(const PositionHFT &position) {
    std::cout << "ImbalanceAndLarge::setPosition" << std::endl;
    std::cout << "id " << position.id << std::endl;
    std::cout << "side " << position.side << std::endl;
    std::cout << "size " << position.size << std::endl;
}

void ImbalanceAndLarge::onMarketUpdate() {
    //! NOTE: Крупная сделка - по большей части просто подтверждение.
    if (publicTradeIsUpdate_) {
        checkSignalTrades();
        publicTradeIsUpdate_ = false;
    }

    checkSignalTotal();

    if (signalTotal_.has_value() && (signalTotal_.value() == Side_Buy)) {
        tradeManager_.makeTrade(currentBestBidPrice_, Side_Buy);
    } else if (signalTotal_.has_value() && (signalTotal_.value() == Side_Sell)) {
        tradeManager_.makeTrade(currentBestBidPrice_, Side_Sell);
    }

    // if (signalDisbalance_.has_value() && signalTrade_.has_value() &&
    //         countSignal_.load() >= settings::countSignal) {
    //     if (signalDisbalance_.value() == Side_Buy && signalTrade_.value() == Side_Buy) {
    //         tradeManager_.makeTrade(currentBestBidPrice_, Side_Buy);
    //         signalTotal_ = Side_Buy;
    //     } else if (signalDisbalance_.value() == Side_Sell && signalTrade_.value() == Side_Sell) {
    //         tradeManager_.makeTrade(currentBestAskPrice_, Side_Sell);
    //         signalTotal_ = Side_Sell;
    //     } else {
    //         countSignal_.store(0);
    //         signalTotal_ = std::nullopt;
    //     }
    // } else {
    //     signalTotal_ = std::nullopt;
    // }

    //! TODO: Если встретился противоположный сигнал.
    // Подумать - закрывать только по противоположному дисбалансу или по полному противоположному сигналу?
    // if (signalDisbalance_.has_value()) {
    //     tradeManager_.checkInverseSignal(signalDisbalance_.value());
    // }
    //! TODO: Здесь закрытие по полному противоположному сигналу.
    //! TODO: Пока противоположный сигнал никак не учитываем
    // if (signalInverseDisbalance_.has_value() &&
    //         countInverseSignal_.load() >= settings::countInverseSignal) {
    //     tradeManager_.checkInverseSignal(signalInverseDisbalance_.value());
    // }

    // if (signalDisbalance_.has_value() && countSignal_.load() >= settings::countSignal) {
    //     tradeManager_.checkInverseSignal(signalDisbalance_.value());
    // }
    if (tradeManager_.hasOpenTrade() && signalTotal_.has_value()) {
        tradeManager_.checkInverseSignal(signalTotal_.value());
    }
}

void ImbalanceAndLarge::checkSignalTrades() {
    //! TODO: Нужно внимательно проследить, что новые данные о сделках не добавятся.
    size_t countPlusTick = 0;
    size_t countminusTick = 0;

    double plusTickQtu = 0.0;
    double minusTickQtu = 0.0;

    for (const auto &trade : publicTrades_.back()) {
        if (trade.L == PublicTrade::TickDirection_PlusTick) {
            countPlusTick++;
            plusTickQtu += trade.v;
        }
        if (trade.L == PublicTrade::TickDirection_MinusTick) {
            countminusTick++;
            minusTickQtu += trade.v;
        }
    }

    // if (countPlusTick > countminusTick) {
    //     signalTrade_ = Side_Buy;
    // } else if (countminusTick > countPlusTick) {
    //     signalTrade_ = Side_Sell;
    // } else {
    //     signalTrade_ = std::nullopt;
    // }

    if (plusTickQtu > minusTickQtu) {
        signalTrade_ = Side_Buy;
    } else if (minusTickQtu > plusTickQtu) {
        signalTrade_ = Side_Sell;
    } else {
        signalTrade_ = std::nullopt;
    }
}

void ImbalanceAndLarge::checkSignalTotal() {
    if (!imbalanceIndicator_.hasSignal()) {
        signalTotal_ = std::nullopt;
    }

    signalTotal_ = imbalanceIndicator_.getSignal();
}
