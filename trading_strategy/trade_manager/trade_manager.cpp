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

inline double CalcLimitPrice(const double price, const Side &side) {
    double delta = settings::spaceToLimitPrice * price;
    if (side == Side_Buy) {
        delta *= -1.0;
    }
    return price + delta;
}

} // namespace

TradeManager::TradeManager() :
orderSender_(),
currentTradeSide_(std::nullopt),
// priceOpen_(0.0),
// priceDlose_(0.0)
openPrice(0.0),
currentPrice(0.0),
waitOpenLimitOrder_(false) {
}

void TradeManager::setOrderSender(OrderSender orderSender) {
    //! TODO: Добавить nutex на установку отправителя ордеров
    orderSender_ = orderSender;
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
    } else if (orderTypeId == 4) { // выход
        checkCloseOrder(it->second);
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
    std::cout << "time = " << std::chrono::steady_clock::now().time_since_epoch() << " ";
    currentTrade_.clearStatuses();
    currentTrade_.makeTradeByLimitOrder(CalcLimitPrice(price, side), side);
    openPrice = price;

    if (!orderSender_) {
        return;
    }

    if (orderSender_(currentTrade_.openLimitOrder)) {
        // Поставили таймер - в течение некоторого времени должно прийти
        // подтверждение взятия ордера.
        timePlaceOrder_ = std::chrono::steady_clock::now();
        currentTradeSide_ = side;
        waitOpenLimitOrder_ = true;
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

void TradeManager::closeBuyTrade() {
    const double limitPrice = CalcLimitPrice(currentPrice, Side_Sell);
    currentTrade_.makeCloseLimitOrder(limitPrice, Side_Sell);
    currentTradeSide_ = std::nullopt;
}

void TradeManager::closeSellTrade() {
    const double limitPrice = CalcLimitPrice(currentPrice, Side_Buy);
    currentTrade_.makeCloseLimitOrder(limitPrice, Side_Buy);
    currentTradeSide_ = std::nullopt;
}

void TradeManager::checkInverseSignal(const Side &side) {
    // Если лимитный ордер на вход не выполнен, то сделка не открыта и обратный сигнал не рассматриваем
    if (currentTrade_.ordersStatus[currentTrade_.orderOpenTrade.req_id] != OrderStatus_Filled) {
        return;
    }

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

    if (std::chrono::steady_clock::now() - timePlaceOrder_ > settings::waitLimitOrderTime) {
        std::cout << "limit order not released " << std::endl;
        currentTrade_.cancelOrder(currentTrade_.openLimitOrder.req_id);
        return;
    }
}

void TradeManager::openTrade() {
}

void TradeManager::closeTrade(const double endPrice) {
    double profit = endPrice - openPrice - 43.0;
    if (currentTradeSide_.value() == Side_Sell) {
        profit *= -1.0;
    }
    if (profit > 0.0) {
        countPositive++;
    }
    double profitPercent = profit / endPrice;

    totalCount++;
    totalProfit += profit;
    totalProfitPercent += profitPercent;
    std::cout << "time = " << std::chrono::steady_clock::now().time_since_epoch();
    std::cout << " profit: " << profit << std::endl;
    std::cout << "totalCount: " << totalCount << " totalProfit: " << totalProfit
              << " stopLossCount: " << stopLossCount << " takeProfitCount: " << takeProfitCount
              << " inverseSignalCount: " << inverseSignalCount << " timeoutCount: " << timeoutCount
              << " win rate = " << (double)countPositive / totalCount << std::endl;
    currentTradeSide_ = std::nullopt;
}

void TradeManager::checkMainOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) { // ордер исполнен
        // Ордер исполнен. Вошли в сделку. Можно выставлять TP и SL
        currentTrade_.makeTPSLOrders(currentPrice, currentTradeSide_.value());
        startTrade = std::chrono::steady_clock::now();
        waitOpenLimitOrder_ = false;
    }
}

void TradeManager::checkStopLoss(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTrade_.cancelAllOrders();
        currentTradeSide_ = std::nullopt;
    }
}

void TradeManager::checkTakeProfit(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTrade_.cancelAllOrders();
        currentTradeSide_ = std::nullopt;
    }
}

void TradeManager::checkCloseOrder(const OrderStatus &orderStatus) {
    if (orderStatus == OrderStatus_Filled) {
        currentTrade_.cancelAllOrders();
        currentTradeSide_ = std::nullopt;

        //! TODO: Если подтверждение не пришло в течение какого-то времени, то ставим
        // currentTradeSide_
    }
}
