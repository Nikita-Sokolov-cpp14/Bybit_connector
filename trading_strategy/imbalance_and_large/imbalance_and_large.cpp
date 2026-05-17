#include "imbalance_and_large.h"

#include <iostream>

#include "settings.h"
#include "data_structures/public_trade.h"

ImbalanceAndLarge::ImbalanceAndLarge() :
disbalance_(0),
orderbookIsUpdate_(false),
publicTradeIsUpdate_(false),
midPrices_(settings::historyMidPriceSize),
publicTrades_(settings::historyPublicTradeSize),
signalDisbalance_(std::nullopt),
signalTrade_(std::nullopt),
signalTotal_(std::nullopt) {
}

void ImbalanceAndLarge::setOrderbook(const OrderBook &orderbook) {
    calcDisbalance(orderbook);
    const double midPrice = (orderbook.asks.begin()->first + orderbook.bids.begin()->first) / 2.0;
    midPrices_.push_back(midPrice);
    orderbookIsUpdate_.store(true);
    newData_.store(true);
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

void ImbalanceAndLarge::calcDisbalance(const OrderBook &orderbook) {
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
        disbalance_.store(1.0);
        return;
    }

    disbalance_.store(sumVolBids / sumVolAsks);
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

    if (signalDisbalance_.has_value() && signalTrade_.has_value()) {
        if (signalDisbalance_.value() == Side_Buy && signalTrade_.value() == Side_Buy) {
            // std::cout << "ImbalanceAndLarge: total buy" << std::endl;
            tradeManager_.makeBuyTrade();
            signalTotal_ = Side_Buy;
        } else if (signalDisbalance_.value() == Side_Sell && signalTrade_.value() == Side_Sell) {
            // std::cout << "ImbalanceAndLarge: total sell" << std::endl;
            tradeManager_.makeSellTrade();
            signalTotal_ = Side_Sell;
        } else {
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
    if (signalTotal_.has_value()) {
        tradeManager_.checkInverseSignal(signalTotal_.value());
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
    if (disbalance_ >= settings::buyDisbalance) {
        signalDisbalance_ = Side_Buy;
    } else if (disbalance_ <= settings::sellDisbalance) {
        signalDisbalance_ = Side_Sell;
    } else {
        signalDisbalance_ = std::nullopt;
    }
}
