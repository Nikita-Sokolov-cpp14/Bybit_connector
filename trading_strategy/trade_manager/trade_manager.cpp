#include "trade_manager.h"

TradeManager::TradeManager() :
orderSender_(),
currentTradeSide_(std::nullopt),
priceOpen_(0.0),
priceDlose_(0.0) {
}

void TradeManager::setOrderSender(OrderSender orderSender) {
    //! TODO: Добавить nutex на установку отправителя ордеров
    orderSender_ = orderSender;
}

void TradeManager::makeTrade(const double price, const Side &side) {
    if (hasOpenTrade()) {
        return;
    }
    currentTradeSide_ = side;
    currentTrade_.makeTrade(price, side);

    bool openIsPlace = false;
    bool stopIsPlace = false;
    bool takeIsPlace = false;
    // if (orderSender_) {
    //     openIsPlace = orderSender_(currentTrade_.orderOpenTrade);
    //     stopIsPlace = orderSender_(currentTrade_.stopLoss);
    //     takeIsPlace = orderSender_(currentTrade_.takeProfit);
    // }
}

bool TradeManager::hasOpenBuyTrade() const {
    if (!hasOpenTrade()) {
        return false;
    }

    return currentTradeSide_.value() == Side_Buy;
}

bool TradeManager::hasOpenSellTrade() const {
    if (!hasOpenTrade()) {
        return false;
    }

    return currentTradeSide_.value() == Side_Sell;
}

void TradeManager::closeBuyTrade() {
    std::cout << " TradeManager::closeBuyTrade " << std::endl;
    currentTradeSide_ = std::nullopt;
}

void TradeManager::closeSellTrade() {
    std::cout << " TradeManager::closeSellTrade " << std::endl;
    currentTradeSide_ = std::nullopt;
}

void TradeManager::checkInverseSignal(const Side &side) {
    if (hasOpenBuyTrade() && side == Side_Sell) {
        closeBuyTrade();
    } else if (hasOpenSellTrade() && side == Side_Buy) {
        closeSellTrade();
    }
}

bool TradeManager::hasOpenTrade() const {
    return currentTradeSide_.has_value();
}

void TradeManager::openTrade() {
}

void TradeManager::closeTrade() {
}
