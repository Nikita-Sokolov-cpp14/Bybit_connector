#include "trade_manager.h"
#include "settings.h"
#include "json_parser/base_json_parser.h"

#include <string>

namespace {

double totalProfit = 0;
double totalProfitPercent = 0;
int totalCount = 0;
int countPositive = 0;
int stopLossCount = 0;
int takeProfitCount = 0;
int inverseSignalCount = 0;
int timeoutCount = 0;

int countLimitOpenMiss = 0;
int countLimitOpenFilled = 0;
int countLimitCloseMiss = 0;
int countLimitCloseFilled = 0;
int countCloseMarket = 0;

int maxCountTrades = 3;

inline double CalcLimitPrice(const double price, const Side &side) {
    double delta = settings::spaceToLimitPrice * price;
    if (side == Side_Buy) {
        delta *= -1.0;
    }
    return price + delta;
}

} // namespace

TradeManager::TradeManager() :
currentTradeSide_(std::nullopt),
// priceOpen_(0.0),
// priceDlose_(0.0)
openPrice(0.0),
closePrice(0.0),
currentPrice(0.0),
waitOpenLimitOrder_(false),
waitCloseLimitOrder_(false),
waitCloseMarketOrder_(false),
waitCancelCloseLimitOrder_(false),
tradeIsOpen_(false) {
}

void TradeManager::setOrderSender(Trade::OrderSender orderSender) {
    //! TODO: Добавить nutex на установку отправителя ордеров
    currentTrade_.orderSender = orderSender;
}

void TradeManager::setOrder(const OrderMainData &order) {
    uint64_t orderLinkId = convertTo<uint64_t>(order.orderLinkId);
    auto it = currentTrade_.ordersStatus.find(orderLinkId);
    if (it == currentTrade_.ordersStatus.end()) {
        std::cout << "TradeManager::setOrder: Unknown order" << std::endl;
        return;
    } else {
        it->second = order.orderStatus;
    }

    //! TODO: Заменить на более читаемое
    const uint64_t orderTypeId = orderLinkId % 10;
    if (orderTypeId == 1) { // Основной ордер
        checkMainOrder(it->second);
        // } else if (orderTypeId == 2) { // стоп
        //     checkStopLoss(it->second);
        // } else if (orderTypeId == 3) { // тейк
        //     checkTakeProfit(it->second);
    } else if (orderTypeId == 4) { // выход по лимитному ордеру
        checkCloseLimitOrder(it->second);
    } else if (orderTypeId == 5) { // выход по рыночному ордеру
        checkCloseMarketOrder(it->second);
    }
}

void TradeManager::setPosition(const PositionHFT &position) {
}

void TradeManager::makeTrade(const double price, const Side &side) {
    if (totalCount > maxCountTrades) {
        return;
    }

    if (hasOpenTrade()) {
        if (sideIsEqual(side)) {
            startTrade = std::chrono::steady_clock::now();
        }
        return;
    }

    currentTradeSide_ = std::nullopt;
    std::cout << "side: " << side
              << " time: " << std::chrono::steady_clock::now().time_since_epoch() << std::endl;
    currentTrade_.clearStatuses();
    if (currentTrade_.makeTradeByLimitOrder(CalcLimitPrice(price, side), side)) {
        timePlaceOpenOrder_ = std::chrono::steady_clock::now();
        currentTradeSide_ = side;
        waitOpenLimitOrder_ = true;
    } else {
        std::cout << "TradeManager::makeTrade: can't make trade" << std::endl;
    }
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

void TradeManager::checkInverseSignal(const Side &side) {
    // Если лимитный ордер на вход не выполнен, то сделка не открыта и обратный сигнал не рассматриваем
    if (!hasOpenTrade()) {
        return;
    }

    if (waitOpenLimitOrder_) {
        if (sideIsEqual(side)) {
            timePlaceOpenOrder_ = std::chrono::steady_clock::now();
            //! NOTE: Нужно переместить ордер ближе к текущей цене.
            // moveOpenOrderToPrice();
        } else {
            cancelOpenLimitOrder();
        }
    }

    if (waitCloseLimitOrder_ && checkProfit()) {
        if (sideIsEqual(side)) {
            timePlaceCloseOrder_ = std::chrono::steady_clock::now();
        }
    }

    if (!tradeIsOpen_) {
        return;
    }

    if (currentTradeSide_ == side) {
        return;
    }

    if (waitCloseLimitOrder_) {
        return;
    }

    closeTrade();
}

void TradeManager::closeTrade() {
    Side side = (currentTradeSide_ == Side_Buy) ? Side_Sell : Side_Buy;

    const double limitPrice = CalcLimitPrice(currentPrice, side);
    if (!currentTrade_.makeCloseLimitOrder(limitPrice, side)) {
        std::cout << "TradeManager::closeTrade: can't place close limit order" << std::endl;
        return;
    }
    waitCloseLimitOrder_ = true;
    timePlaceCloseOrder_ = std::chrono::steady_clock::now();
}

bool TradeManager::hasOpenTrade() const {
    return currentTradeSide_.has_value();
}

void TradeManager::checkCurrentPrice(const double price) {
    currentPrice = price;
    if (!hasOpenTrade()) {
        return;
    }

    if (tradeIsOpen_ && (std::chrono::steady_clock::now() - startTrade > settings::tradeTimeOut) &&
            !waitCloseLimitOrder_ && !waitCloseMarketOrder_) {
        std::cout << "close by timeout " << std::endl;
        timeoutCount++;
        closeTrade();
        return;
    }

    //! NOTE: Ожидаем взятие лимитного ордера и прошло время ожидания больше допустимого.
    if (waitOpenLimitOrder_ &&
            (std::chrono::steady_clock::now() - timePlaceOpenOrder_ >
                    settings::waitOpenLimitOrderTime)) {
        countLimitOpenMiss++;
        std::cout << "limit open order not released " << std::endl;
        cancelOpenLimitOrder();
        return;
    }

    if (waitCloseLimitOrder_ && !waitCloseMarketOrder_ &&
            (std::chrono::steady_clock::now() - timePlaceCloseOrder_ >
                    settings::waitCloseLimitOrderTime)) {
        countLimitCloseMiss++;
        waitCloseLimitOrder_ = false;
        std::cout << "limit close order not released " << std::endl;
        waitCancelCloseLimitOrder_ = true;
        currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id);
        return;
    }

    if (!waitCloseLimitOrder_ && !waitCancelCloseLimitOrder_ && !waitCloseMarketOrder_) {
        checkSLTP();
    }
}

void TradeManager::checkMainOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) { // ордер исполнен
        countLimitOpenFilled++;
        startTrade = std::chrono::steady_clock::now();
        openPrice = currentTrade_.openLimitOrder.price;
        tradeIsOpen_ = true;
        //! NOTE: На случай, если отправили отмену ордера, а он в этот момент исполнился.
        if (waitOpenLimitOrder_ == false) {
            std::cout << "limit open order canceled but released" << std::endl;
            countLimitOpenMiss--;
            currentTradeSide_ = currentTrade_.openLimitOrder.side;
        }
        // Ордер исполнен. Вошли в сделку. Можно выставлять TP и SL
        // currentTrade_.makeTPSLOrders(currentPrice, currentTradeSide_.value());
        currentTrade_.calcSLTP(currentTrade_.openLimitOrder.price, currentTradeSide_.value());
        waitOpenLimitOrder_ = false;
    } else if (orderStatus == OrderStatus_Cancelled) {
        currentTradeSide_ = std::nullopt;
    }
}

// void TradeManager::checkStopLoss(const OrderStatus &orderStatus) {
//     if (orderStatus == OrderStatus_Filled) {
//         currentTradeSide_ = std::nullopt;

//         if (!currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id) ||
//                 !currentTrade_.cancelOrder(currentTrade_.takeProfit.req_id)) {
//             std::cout << "TradeManager::checkStopLoss: can't send orders" << std::endl;
//             return;
//         }
//     }
// }

// void TradeManager::checkTakeProfit(const OrderStatus &orderStatus) {
//     if (orderStatus == OrderStatus_Filled) {
//         currentTradeSide_ = std::nullopt;

//         if (!currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id) ||
//                 !currentTrade_.cancelOrder(currentTrade_.stopLoss.req_id)) {
//             std::cout << "TradeManager::checkTakeProfit: can't send orders" << std::endl;
//             return;
//         }
//     }
// }

void TradeManager::checkCloseLimitOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        closePrice = currentTrade_.closeLimitOrder.price;
        tradeIsOpen_ = false;
        countLimitCloseFilled++;
        printData();
        currentTradeSide_ = std::nullopt;
        waitCancelCloseLimitOrder_ = false;

        //! NOTE: На случай, если отправили отмену ордера, а он в этот момент исполнился.
        if (!waitCloseLimitOrder_) {
            std::cout << "limit close order canceled but released" << std::endl;
            countLimitCloseMiss--;
        }
    } else if (orderStatus == OrderStatus_Cancelled) {
        waitCancelCloseLimitOrder_ = false;
        Side side = (currentTradeSide_ == Side_Buy) ? Side_Sell : Side_Buy;
        if (currentTrade_.makeCloseMarketOrder(side)) {
            waitCloseMarketOrder_ = true;
            closePrice = currentPrice;
        }
    }

    // if (!currentTrade_.cancelOrder(currentTrade_.takeProfit.req_id) ||
    //         !currentTrade_.cancelOrder(currentTrade_.stopLoss.req_id)) {
    //     std::cout << "TradeManager::checkCloseLimitOrder: can't send orders" << std::endl;
    // }
}

void TradeManager::checkCloseMarketOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        tradeIsOpen_ = false;
        countCloseMarket++;
        printData();
        currentTradeSide_ = std::nullopt;
        waitCloseMarketOrder_ = false;
    }
}

void TradeManager::checkSLTP() {
    if (!hasOpenTrade()) {
        return;
    }

    if (currentTrade_.ordersStatus[currentTrade_.openLimitOrder.req_id] != OrderStatus_Filled) {
        return;
    }

    if (currentTradeSide_ == Side_Sell && (currentPrice >= currentTrade_.stopLossPrice)) {
        std::cout << "TradeManager::checkSLTP: stop loss sell trade" << std::endl;
        if (currentTrade_.makeCloseMarketOrder(Side_Buy)) {
            waitCloseMarketOrder_ = true;
        }
    } else if (currentTradeSide_ == Side_Buy && (currentPrice <= currentTrade_.stopLossPrice)) {
        std::cout << "TradeManager::checkSLTP: stop loss buy trade" << std::endl;
        if (currentTrade_.makeCloseMarketOrder(Side_Sell)) {
            waitCloseMarketOrder_ = true;
        }
    }

    if (currentTradeSide_ == Side_Sell && (currentPrice <= currentTrade_.takeProfitPrice)) {
        std::cout << "TradeManager::checkSLTP: take profit sell trade" << std::endl;
        closeTrade();
    } else if (currentTradeSide_ == Side_Buy && (currentPrice >= currentTrade_.takeProfitPrice)) {
        std::cout << "TradeManager::checkSLTP: take profit buy trade" << std::endl;
        closeTrade();
    }
}

void TradeManager::cancelOpenLimitOrder() {
    waitOpenLimitOrder_ = false;
    currentTrade_.cancelOrder(currentTrade_.openLimitOrder.req_id);
    currentTradeSide_ = std::nullopt;
}

bool TradeManager::sideIsEqual(const Side &side) {
    if ((currentTradeSide_ == Side_Buy && side == Side_Buy) ||
            (currentTradeSide_ == Side_Sell && side == Side_Sell)) {
        return true;
    }

    return false;
}

void TradeManager::moveOpenOrderToPrice() {
    if (!currentTrade_.replaceLimitOpenOrder(
                CalcLimitPrice(currentPrice, currentTradeSide_.value()))) {
        std::cout << "TradeManager::moveOpenOrderToPrice: can't replace order" << std::endl;
    }
}

bool TradeManager::checkProfit() {
    if ((currentTradeSide_ == Side_Buy) && (currentPrice > openPrice)) {
        return true;
    } else if ((currentTradeSide_ == Side_Sell) && (currentPrice < openPrice)) {
        return true;
    }

    return false;
}

void TradeManager::printData() {
    double profit = closePrice - openPrice;
    if (currentTradeSide_ == Side_Sell) {
        profit *= -1;
    }
    totalProfit += profit;
    totalCount++;

    std::cout << "profit: " << profit << " total profit: " << totalProfit
              << " count open limit: " << countLimitOpenFilled
              << " miss open: " << countLimitOpenMiss << " close limit: " << countLimitCloseFilled
              << " close market: " << countCloseMarket
              << " miss close limit: " << countLimitCloseMiss << std::endl;
    std::cout << "===============" << std::endl;
}
