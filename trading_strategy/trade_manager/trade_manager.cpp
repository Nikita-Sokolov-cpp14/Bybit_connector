#include "trade_manager.h"

#include "settings.h"

namespace {

double totalProfit = 0;
double totalProfitPercent = 0;
int totalCount = 0;
int countPositive = 0;
int stopLossCount = 0;
int takeProfitCount = 0;
int inverseSignalCount = 0;
int timeoutCount = 0;

} // namespace

TradeManager::TradeManager() :
orderSender_(),
currentTradeSide_(std::nullopt),
// priceOpen_(0.0),
// priceDlose_(0.0)
openPrice(0.0),
currentPrice(0.0) {
}

void TradeManager::setOrderSender(OrderSender orderSender) {
    //! TODO: Добавить nutex на установку отправителя ордеров
    orderSender_ = orderSender;
}

void TradeManager::makeTrade(const double price, const Side &side) {
    if (hasOpenTrade()) {
        if (currentTradeSide_.value() == Side_Buy && side == Side_Buy) {
            startTrade = std::chrono::steady_clock::now();
        } else if (currentTradeSide_.value() == Side_Sell && side == Side_Sell) {
            startTrade = std::chrono::steady_clock::now();
        }

        return;
    }
    std::cout << "time = " << std::chrono::steady_clock::now().time_since_epoch() << " ";
    startTrade = std::chrono::steady_clock::now();
    openPrice = price;

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
    std::cout << " TradeManager::closeBuyTrade "
              << " time = " << std::chrono::steady_clock::now().time_since_epoch() << std::endl;
    inverseSignalCount++;
    closeTrade(currentPrice);
    currentTradeSide_ = std::nullopt;
}

void TradeManager::closeSellTrade() {
    std::cout << " TradeManager::closeSellTrade "
              << " time = " << std::chrono::steady_clock::now().time_since_epoch() << std::endl;
    inverseSignalCount++;
    closeTrade(currentPrice);
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

void TradeManager::checkCurrentPrice(const double price) {
    currentPrice = price;
    if (!hasOpenTrade()) {
        return;
    }

    if (std::chrono::steady_clock::now() - startTrade > settings::tradeTimeOut) {
        std::cout << "close by timeout " << std::endl;
        timeoutCount++;
        closeTrade(price);
        return;
    }

    switch (currentTradeSide_.value()) {
        case Side_Buy:
            if (price < currentTrade_.stopLossPrice) {
                std::cout << "closeTrade: Buy stop loss" << std::endl;
                stopLossCount++;
                closeTrade(price);
                return;
            }
            break;
        case Side_Sell:
            if (price > currentTrade_.stopLossPrice) {
                std::cout << "closeTrade: Sell stop loss" << std::endl;
                closeTrade(price);
                stopLossCount++;
                return;
            }
            break;
        default:
            break;
    }
}

void TradeManager::openTrade() {
}

void TradeManager::closeTrade(const double endPrice) {
    double profit = endPrice - openPrice;
    if (currentTradeSide_.value() == Side_Sell) {
        profit *= -1.0;
    }
    if (profit > 0.0) {
        countPositive++;
    }

    profit -= 43.0;
    double profitPercent = profit / endPrice;

    totalCount++;
    totalProfit += profit;
    totalProfitPercent += profitPercent;
    std::cout << "time = " << std::chrono::steady_clock::now().time_since_epoch();
    std::cout << " profit: " << profit << std::endl;
    std::cout << "totalCount: " << totalCount << " totalProfit: " << totalProfit
              << " stopLossCount: " << stopLossCount << " takeProfitCount: " << takeProfitCount
              << " inverseSignalCount: " << inverseSignalCount << " timeoutCount: " << timeoutCount
              << " win rate = " << (double)countPositive / totalCount
              << std::endl;
    currentTradeSide_ = std::nullopt;
}
