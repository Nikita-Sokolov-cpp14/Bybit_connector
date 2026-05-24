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
currentPrice(0.0),
waitOpenLimitOrder_(false),
waitCloseLimitOrder_(false),
waitCloseMarketOrder_(false) {
}

void TradeManager::setOrderSender(Trade::OrderSender orderSender) {
    //! TODO: Добавить nutex на установку отправителя ордеров
    currentTrade_.orderSender = orderSender;
}

void TradeManager::setOrder(const OrderHFT &order) {
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
    } else if (orderTypeId == 2) { // стоп
        checkStopLoss(it->second);
    } else if (orderTypeId == 3) { // тейк
        checkTakeProfit(it->second);
    } else if (orderTypeId == 4) { // выход по лимитному ордеру
        checkCloseLimitOrder(it->second);
    } else if (orderTypeId == 5) { // выход по рыночному ордеру
        checkCloseMarketOrder(it->second);
    }
}

void TradeManager::setPosition(const PositionHFT &position) {
}

void TradeManager::makeTrade(const double price, const Side &side) {
    if (hasOpenTrade()) {
        if ((currentTradeSide_.value() == Side_Buy && side == Side_Buy) ||
                (currentTradeSide_.value() == Side_Sell && side == Side_Sell)) {
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
    if (currentTrade_.ordersStatus[currentTrade_.openLimitOrder.req_id] != OrderStatus_Filled) {
        return;
    }

    if (!hasOpenTrade()) {
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
}

bool TradeManager::hasOpenTrade() const {
    return currentTradeSide_.has_value();
}

void TradeManager::checkCurrentPrice(const double price) {
    currentPrice = price;
    if (!hasOpenTrade()) {
        return;
    }

    if ((std::chrono::steady_clock::now() - startTrade > settings::tradeTimeOut) &&
            !waitCloseLimitOrder_ && !waitCloseMarketOrder_) {
        std::cout << "close by timeout " << std::endl;
        timeoutCount++;
        closeTrade();
        return;
    }

    //! NOTE: Ожидаем взятие лимитного ордера и прошло время ожидания больше допустимого.
    if (waitOpenLimitOrder_ &&
            (std::chrono::steady_clock::now() - timePlaceOpenOrder_ >
                    settings::waitLimitOrderTime)) {
        countLimitOpenMiss++;
        std::cout << "limit open order not released " << std::endl;
        waitOpenLimitOrder_ = false;
        currentTrade_.cancelOrder(currentTrade_.openLimitOrder.req_id);
        return;
    }

    if (waitCloseLimitOrder_ &&
            (std::chrono::steady_clock::now() - timePlaceCloseOrder_ >
                    settings::waitLimitOrderTime)) {
        countLimitCloseMiss++;
        waitCloseLimitOrder_ = false;
        std::cout << "limit close order not released " << std::endl;
        currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id);
        return;
    }
}

void TradeManager::checkMainOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) { // ордер исполнен
        countLimitOpenFilled++;
        // Ордер исполнен. Вошли в сделку. Можно выставлять TP и SL
        currentTrade_.makeTPSLOrders(currentPrice, currentTradeSide_.value());
        startTrade = std::chrono::steady_clock::now();
        openPrice = currentTrade_.openLimitOrder.price;
        //! NOTE: На случай, если отправили отмену ордера, а он в этот момент исполнился.
        if (waitOpenLimitOrder_ == false) {
            std::cout << "limit open order canceled but released" << std::endl;
            countLimitOpenMiss--;
            currentTradeSide_ = currentTrade_.openLimitOrder.side;
        }
        waitOpenLimitOrder_ = false;
    } else if (orderStatus == OrderStatus_Cancelled) {
        currentTradeSide_ = std::nullopt;
    }
}

void TradeManager::checkStopLoss(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTradeSide_ = std::nullopt;

        if (!currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id) ||
                !currentTrade_.cancelOrder(currentTrade_.takeProfit.req_id)) {
            std::cout << "TradeManager::checkStopLoss: can't send orders" << std::endl;
            return;
        }
    }
}

void TradeManager::checkTakeProfit(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTradeSide_ = std::nullopt;

        if (!currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id) ||
                !currentTrade_.cancelOrder(currentTrade_.stopLoss.req_id)) {
            std::cout << "TradeManager::checkTakeProfit: can't send orders" << std::endl;
            return;
        }
    }
}

void TradeManager::checkCloseLimitOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTradeSide_ = std::nullopt;

        //! NOTE: На случай, если отправили отмену ордера, а он в этот момент исполнился.
        if (!waitCloseLimitOrder_) {
            std::cout << "limit close order canceled but released" << std::endl;
            countLimitCloseMiss--;
        }
    } else if (orderStatus == OrderStatus_Cancelled) {
        Side side = (currentTradeSide_ == Side_Buy) ? Side_Sell : Side_Buy;
        currentTrade_.makeCloseMarketOrder(currentPrice, side);
        waitCloseMarketOrder_ = true;
    }

    if (!currentTrade_.cancelOrder(currentTrade_.takeProfit.req_id) ||
            !currentTrade_.cancelOrder(currentTrade_.stopLoss.req_id)) {
        std::cout << "TradeManager::checkCloseLimitOrder: can't send orders" << std::endl;
    }
}

void TradeManager::checkCloseMarketOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTradeSide_ = std::nullopt;
        waitCloseMarketOrder_ = false;
    }
}
