#include "imbalance_and_large.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

#include "settings.h"
#include "data_structures/public_trade.h"

ImbalanceAndLarge::ImbalanceAndLarge() :
disbalanceAverage_(0),
orderbookIsUpdate_(false),
publicTradeIsUpdate_(false),
needCheckCurPrice_(false),
orderIsUpdate_(false),
midPrices_(settings::historyMidPriceSize),
publicTrades_(settings::historyPublicTradeSize),
currentBestAskPrice_(0.0),
currentBestBidPrice_(0.0),
currentMidPrice_(0.0),
// signalTfi_(std::nullopt),
// signalObi_(std::nullopt),
signalTotal_(std::nullopt),
// orderBookImbalance_(),
// tradeFlowImbalance_(),
loger_("../log_files/total_signals.csv") {
    loger_.recordValue("ts");
    loger_.recordValue("mid_price");
    // loger_.recordValue("signal_obi");
    // loger_.recordValue("signal_tfi");
    loger_.recordValue("signal_combined");
    loger_.endStr();
}

ImbalanceAndLarge::~ImbalanceAndLarge() {
    stop();
}

void ImbalanceAndLarge::setOrderbook(const OrderBook &orderbook) {
    if (orderbook.asks.empty() || orderbook.bids.empty()) {
        return;
    }

    // orderBookImbalance_.setOrderbook(orderbook);
    currentMidPrice_.store((orderbook.asks.begin()->first + orderbook.bids.begin()->first) / 2.0);
    currentBestAskPrice_.store(orderbook.asks.begin()->first);
    currentBestBidPrice_.store(orderbook.bids.begin()->first);
    midPrices_.push_back(currentMidPrice_.load());
    // tradeFlowImbalance_.setMidPrice(currentMidPrice_.load());
    priceFlowDivergence_.setMidPrice(currentMidPrice_.load());

    needCheckCurPrice_.store(true);

    notifyNewData();
}

void ImbalanceAndLarge::setPublicTradeData(const PublicTrade &publicTrade) {
    // tradeFlowImbalance_.setPublicTrade(publicTrade);
    priceFlowDivergence_.setPublicTrade(publicTrade);
    //! TODO: После этой опреации у publicTrade больше нет данных.
    //! TODO: Нужно успеть обработать обновление сделок до прихода новых.
    // Предполагается, что onMarketUpdate работает меньше 1 мс. За это время новая
    // информация не поступит.
    // publicTrades_.push_back(std::move(publicTradeData));
    publicTradeIsUpdate_.store(true);
    notifyNewData();
}

void ImbalanceAndLarge::setOrder(const OrderHFT &order) {
    // std::cout << "ImbalanceAndLarge::setOrder" << std::endl;
    // std::cout << "id " << order.id << std::endl;
    // std::cout << "orderLinkId " << order.orderLinkId << std::endl;
    // std::cout << "side " << order.side << std::endl;
    // std::cout << "orderStatus " << order.orderStatus << std::endl;
    // std::cout << "orderType " << order.orderType << std::endl;
    // std::cout << "price " << order.price << std::endl;
    // std::cout << "qty " << order.qty << std::endl;

    OrderEvent orderEvent;
    orderEvent.orderLinkId = std::string(order.orderLinkId);
    orderEvent.orderId = std::string(order.orderId);
    orderEvent.side = order.side;
    orderEvent.status = order.orderStatus;
    orderEvent.price = order.price;
    orderEvent.qty = order.qty;
    orderEvent.avgPrice = order.avgPrice;
    orderEvent.cumExecQty = order.cumExecQty;
    orderEvent.cumExecFee = order.cumExecFee;
    orderEvent.leavesQty = order.leavesQty;
    orderEvent.updatedTime = order.updatedTime;
    orderEvent.recvTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                                    .count();

    {
        std::lock_guard lock(orderQueueMutex_);
        orderQueue_.push_back(std::move(orderEvent));
    }

    orderIsUpdate_.store(true);
    notifyNewData();
}

void ImbalanceAndLarge::setOrderRequestResult(const OrderRequestResult &result) {
    {
        std::lock_guard lock(orderQueueMutex_);
        orderRequestResults_.push_back(result);
    }

    notifyNewData();
}

void ImbalanceAndLarge::drainOrderRequestResults() {
    std::vector<OrderRequestResult> results;
    {
        std::lock_guard lock(orderQueueMutex_);
        results.swap(orderRequestResults_);
    }

    for (const OrderRequestResult &result : results) {
        tradeManager_.onOrderRequestResult(result);
    }
}

void ImbalanceAndLarge::drainOrderUpdates() {
    std::vector<OrderEvent> events;
    {
        std::lock_guard lock(orderQueueMutex_);
        events.swap(orderQueue_);
    }

    for (const OrderEvent &event : events) {
        tradeManager_.setOrder(event);
    }
}

void ImbalanceAndLarge::setPosition(const PositionHFT &position) {
    // std::cout << "ImbalanceAndLarge::setPosition" << std::endl;
    // std::cout << "id " << position.id << std::endl;
    // std::cout << "side " << position.side << std::endl;
    // std::cout << "size " << position.size << std::endl;
    // newData_.store(true);
    // tradeManager_.setPosition(position);
}

void ImbalanceAndLarge::onMarketUpdate() {
    //! Сначала применяем подтверждения ордеров, затем проверяем TP/SL и только
    //! после этого рассматриваем новый сигнал. Так checkCurrentPrice() реально
    //! вызывается каждые ~20 мс, а не только при появлении сигнала.
    drainOrderRequestResults();
    drainOrderUpdates();

    const double midPrice = currentMidPrice_.load();
    if (midPrice > 0.0) {
        tradeManager_.checkCurrentPrice(midPrice);
    }

    //! PFD хранит последний сигнал до следующего пересчёта интервала.
    //! Без проверки версии onMarketUpdate() (20 мс) открывал бы сделку или
    //! продлевал её на каждом обновлении стакана.
    const uint64_t signalVersion = priceFlowDivergence_.getSignalVersion();
    if (signalVersion == lastSignalVersion_) {
        return;
    }
    lastSignalVersion_ = signalVersion;

    // signalTfi_ = tradeFlowImbalance_.getSignal();
    // signalObi_ = orderBookImbalance_.getSignal();
    signalPfd_ = priceFlowDivergence_.getSignal();

    checkSignalTotal();
    // logData();
    if (!signalTotal_.has_value()) {
        return;
    }

    if (signalTotal_.value() == Side_Buy) {
        tradeManager_.makeTrade(currentBestAskPrice_, Side_Buy);
    } else {
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
    // if (tradeManager_.hasOpenTrade() && signalTotal_.has_value()) {
    //     tradeManager_.checkInverseSignal(signalTotal_.value());
    // }
}

void ImbalanceAndLarge::checkSignalTotal() {
    // if ((signalTfi_ == Side_Buy) && (signalObi_ == Side_Buy) &&
    //         (signalPfd_ == Side_Buy)) {
    //     signalTotal_ = Side_Buy;
    // } else if ((signalTfi_ == Side_Sell) && (signalObi_ == Side_Sell) &&
    //         (signalPfd_ == Side_Sell)) {
    //     signalTotal_ = Side_Sell;
    // } else {
    //     signalTotal_ = std::nullopt;
    // }

    if (signalPfd_ == Side_Buy) {
        signalTotal_ = Side_Buy;
    } else if (signalPfd_ == Side_Sell) {
        signalTotal_ = Side_Sell;
    } else {
        signalTotal_ = std::nullopt;
    }
}

void ImbalanceAndLarge::logData() {
    loger_.recordValue(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
                               .count());
    loger_.recordValue(currentMidPrice_.load());
    // loger_.recordValue(signalObi_.value_or(Side_Unknown));
    // loger_.recordValue(signalTfi_.value_or(Side_Unknown));
    loger_.recordValue(signalTotal_.value_or(Side_Unknown));
    loger_.endStr();
}
