#include "trade_manager.h"
#include "settings.h"
#include "json_parser/base_json_parser.h"

#include <chrono>
#include <string>

namespace {

double totalProfit = 0.0;
double totalProfitPercent = 0.0;
int totalCount = 0;
int countPositive = 0;
int stopLossCount = 0;
int takeProfitCount = 0;
int inverseSignalCount = 0;
int timeoutCount = 0;

int countLimitOpenMiss = 0;
int countLimitOpenFilled = 0;
int countMarketOpenFilled = 0;
int countLimitCloseMiss = 0;
int countLimitCloseFilled = 0;
int countCloseMarket = 0;

constexpr double qtyEpsilon = 1e-12;

const char *sideToString(Side side) {
    switch (side) {
        case Side_Buy:
            return "Buy";
        case Side_Sell:
            return "Sell";
        default:
            return "Unknown";
    }
}

const char *orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus_Created:
            return "Created";
        case OrderStatus_New:
            return "New";
        case OrderStatus_PartiallyFilled:
            return "PartiallyFilled";
        case OrderStatus_Filled:
            return "Filled";
        case OrderStatus_Cancelled:
            return "Cancelled";
        case OrderStatus_Rejected:
            return "Rejected";
        case OrderStatus_Expired:
            return "Expired";
        case OrderStatus_Deactivated:
            return "Deactivated";
        case OrderStatus_Triggered:
            return "Triggered";
        default:
            return "Unknown";
    }
}

const char *orderTypeToString(OrderType type) {
    switch (type) {
        case OrderType_Limit:
            return "Limit";
        case OrderType_Market:
            return "Market";
        default:
            return "Unknown";
    }
}

const char *closeReasonToString(CloseReason reason) {
    switch (reason) {
        case CloseReason_TakeProfit:
            return "take_profit";
        case CloseReason_StopLoss:
            return "stop_loss";
        case CloseReason_Timeout:
            return "timeout";
        case CloseReason_InverseSignal:
            return "inverse_signal";
        case CloseReason_Manual:
            return "manual";
        default:
            return "unknown";
    }
}

const char *orderRoleToString(Trade::OrderType type) {
    switch (type) {
        case Trade::OrderType_OpenLimitOrder:
            return "open_limit";
        case Trade::OrderType_OpenMarketOrder:
            return "open_market";
        case Trade::OrderType_CloseLimitOrder:
            return "close_limit";
        case Trade::OrderType_CloseMarketOrder:
            return "close_market";
        default:
            return "unknown";
    }
}

uint64_t systemTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
}

inline double CalcLimitPrice(const double price, const Side &side) {
    double delta = settings::spaceToLimitPrice * price;
    if (side == Side_Buy) {
        delta *= -1.0;
    }
    return price + delta;
}

} // namespace

TradeManager::TradeManager() :
ordersLoger_("../log_files/orders.csv"),
tradesLoger_("../log_files/trades.csv") {
    ordersLoger_.recordValue("ts_local_ms");
    ordersLoger_.recordValue("trade_number");
    ordersLoger_.recordValue("event");
    ordersLoger_.recordValue("role");
    ordersLoger_.recordValue("order_link_id");
    ordersLoger_.recordValue("exchange_order_id");
    ordersLoger_.recordValue("side");
    ordersLoger_.recordValue("order_type");
    ordersLoger_.recordValue("status");
    ordersLoger_.recordValue("requested_price");
    ordersLoger_.recordValue("qty");
    ordersLoger_.recordValue("avg_fill_price");
    ordersLoger_.recordValue("cum_exec_qty");
    ordersLoger_.recordValue("leaves_qty");
    ordersLoger_.recordValue("cum_fee");
    ordersLoger_.recordValue("exchange_updated_ms");
    ordersLoger_.recordValue("local_recv_ms");
    ordersLoger_.recordValue("latency_us");
    ordersLoger_.endStr();

    tradesLoger_.recordValue("trade_number");
    tradesLoger_.recordValue("side");
    tradesLoger_.recordValue("open_time_ms");
    tradesLoger_.recordValue("close_time_ms");
    tradesLoger_.recordValue("hold_ms");
    tradesLoger_.recordValue("entry_type");
    tradesLoger_.recordValue("close_reason");
    tradesLoger_.recordValue("open_price");
    tradesLoger_.recordValue("close_price");
    tradesLoger_.recordValue("qty");
    tradesLoger_.recordValue("notional_usdt");
    tradesLoger_.recordValue("gross_pnl");
    tradesLoger_.recordValue("fees");
    tradesLoger_.recordValue("net_pnl");
    tradesLoger_.recordValue("net_pnl_pct");
    tradesLoger_.recordValue("total_net_pnl");
    tradesLoger_.endStr();
}

void TradeManager::setOrderSender(Trade::OrderSender orderSender) {
    currentTrade_.orderSender = orderSender;
}

void TradeManager::setOrder(const OrderEvent &order) {
    const uint64_t orderLinkId = convertTo<uint64_t>(order.orderLinkId);
    auto it = currentTrade_.ordersStatus.find(orderLinkId);
    if (it == currentTrade_.ordersStatus.end()) {
        std::cout << "TradeManager::setOrder: unknown order " << order.orderLinkId << std::endl;
        return;
    }

    it->second = order.status;

    const Trade::OrderType orderType = currentTrade_.getOrderType(orderLinkId);
    logOrderUpdate(order, orderType);

    if (!currentTradeSide_.has_value()) {
        return;
    }

    switch (orderType) {
        case Trade::OrderType_OpenLimitOrder:
            handleOpenLimitOrder(order);
            break;
        case Trade::OrderType_OpenMarketOrder:
            handleOpenMarketOrder(order);
            break;
        case Trade::OrderType_CloseLimitOrder:
            handleCloseLimitOrder(order);
            break;
        case Trade::OrderType_CloseMarketOrder:
            handleCloseMarketOrder(order);
            break;
        default:
            break;
    }
}

void TradeManager::setPosition(const PositionHFT &position) {
}

void TradeManager::makeTrade(const double price, const Side &side) {
    if (hasOpenTrade()) {
        if (tradeIsOpen_ && sideIsEqual(side)) {
            startTrade_ = std::chrono::steady_clock::now();
        }
        return;
    }

    resetTradeState();
    currentTradeSide_ = std::nullopt;

    if (currentTrade_.makeTradeByLimitOrder(CalcLimitPrice(price, side), side)) {
        currentTradeSide_ = side;
        timePlaceOpenOrder_ = std::chrono::steady_clock::now();
        waitOpenLimitOrder_ = true;
        logOrderSent(currentTrade_.openLimitOrder, "open_limit", true);
    } else {
        logOrderSent(currentTrade_.openLimitOrder, "open_limit", false);
        currentTrade_.clearStatuses();
        resetTradeState();
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
    if (!hasOpenTrade()) {
        return;
    }

    if (waitOpenLimitOrder_) {
        if (sideIsEqual(side)) {
            timePlaceOpenOrder_ = std::chrono::steady_clock::now();
        } else {
            cancelOpenLimitOrder();
        }
    }

    if (!tradeIsOpen_ || sideIsEqual(side) || isCloseInProgress()) {
        return;
    }

    closeTrade(CloseReason_InverseSignal);
}

void TradeManager::closeTrade(CloseReason reason) {
    if (!tradeIsOpen_ || !currentTradeSide_.has_value() || isCloseInProgress()) {
        return;
    }

    closeReason_ = reason;
    pendingCloseQty = (openQty > qtyEpsilon) ? (openQty - closePartiallyFilledQty)
                                             : currentTrade_.openLimitOrder.qty;
    if (pendingCloseQty <= qtyEpsilon) {
        finishTrade();
        return;
    }

    const Side side = (currentTradeSide_ == Side_Buy) ? Side_Sell : Side_Buy;
    const double limitPrice = CalcLimitPrice(currentPrice, side);
    if (!currentTrade_.makeCloseLimitOrder(limitPrice, side, pendingCloseQty)) {
        logOrderSent(currentTrade_.closeLimitOrder, "close_limit", false);
        std::cout << "TradeManager::closeTrade: can't place close limit order" << std::endl;
        return;
    }

    waitCloseLimitOrder_ = true;
    timePlaceCloseOrder_ = std::chrono::steady_clock::now();
    logOrderSent(currentTrade_.closeLimitOrder, "close_limit", true);
}

bool TradeManager::hasOpenTrade() const {
    return currentTradeSide_.has_value();
}

void TradeManager::checkCurrentPrice(const double price) {
    currentPrice = price;
    if (!hasOpenTrade()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    //! Сделка открыта, но превышено максимальное время удержания.
    if (tradeIsOpen_ && (now - startTrade_ > settings::tradeTimeOut) && !isCloseInProgress()) {
        timeoutCount++;
        closeTrade(CloseReason_Timeout);
        return;
    }

    //! Ждём исполнения открывающего лимитного ордера.
    if (waitOpenLimitOrder_ && (now - timePlaceOpenOrder_ > settings::waitOpenLimitOrderTime)) {
        countLimitOpenMiss++;
        std::cout << "TradeManager: open limit timeout, cancel and send market" << std::endl;
        cancelOpenLimitOrder();
        return;
    }

    //! Отмена открывающего ордера отправлена, но подтверждения нет - повторяем отмену.
    if (waitCancelOpenLimitOrder_ &&
            (now - timePlaceOpenOrder_ > settings::waitOpenLimitOrderTime)) {
        if (currentTrade_.cancelOrder(currentTrade_.openLimitOrder.req_id)) {
            timePlaceOpenOrder_ = now;
            logOrderSent(currentTrade_.orderCancel, "cancel_open_limit", true);
        }
        return;
    }

    //! Ждём исполнения закрывающего лимитного ордера.
    if (waitCloseLimitOrder_ && !waitCloseMarketOrder_ &&
            (now - timePlaceCloseOrder_ > settings::waitCloseLimitOrderTime)) {
        countLimitCloseMiss++;
        std::cout << "TradeManager: close limit timeout, cancel and send market" << std::endl;
        cancelCloseLimitOrder();
        return;
    }

    //! Отмена закрывающего ордера отправлена, но подтверждения нет - повторяем отмену.
    if (waitCancelCloseLimitOrder_ &&
            (now - timePlaceCloseOrder_ > settings::waitCloseLimitOrderTime)) {
        if (currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id)) {
            timePlaceCloseOrder_ = now;
            logOrderSent(currentTrade_.orderCancel, "cancel_close_limit", true);
        }
        return;
    }

    if (!isCloseInProgress()) {
        checkSLTP();
    }
}

void TradeManager::handleOpenLimitOrder(const OrderEvent &order) {
    switch (order.status) {
        case OrderStatus_Filled:
            if (tradeIsOpen_) {
                return;
            }
            countLimitOpenFilled++;
            waitOpenLimitOrder_ = false;
            waitCancelOpenLimitOrder_ = false;
            enterTrade(order, false);
            break;
        case OrderStatus_Cancelled:
        case OrderStatus_Rejected:
        case OrderStatus_Expired:
        case OrderStatus_Deactivated:
            if (tradeIsOpen_) {
                return;
            }
            waitOpenLimitOrder_ = false;
            waitCancelOpenLimitOrder_ = false;

            //! Лимитный ордер мог исполниться частично до отмены. В этом случае
            //! считаем, что сделка открыта на исполненный объём, и не отправляем
            //! рыночный ордер на весь qty (иначе получим двойную позицию).
            if (order.cumExecQty > qtyEpsilon) {
                enterTrade(order, false);
            } else if (!waitOpenMarketOrder_) {
                sendOpenMarketFallback();
            }
            break;
        default:
            break;
    }
}

void TradeManager::handleOpenMarketOrder(const OrderEvent &order) {
    switch (order.status) {
        case OrderStatus_Filled:
            if (tradeIsOpen_) {
                return;
            }
            countMarketOpenFilled++;
            waitOpenMarketOrder_ = false;
            enterTrade(order, true);
            break;
        case OrderStatus_Cancelled:
        case OrderStatus_Rejected:
        case OrderStatus_Expired:
        case OrderStatus_Deactivated:
            waitOpenMarketOrder_ = false;
            if (order.cumExecQty > qtyEpsilon) {
                enterTrade(order, true);
            } else {
                std::cout << "TradeManager: market open order not filled, trade cancelled"
                          << std::endl;
                resetTradeState();
                currentTradeSide_ = std::nullopt;
            }
            break;
        default:
            break;
    }
}

void TradeManager::handleCloseLimitOrder(const OrderEvent &order) {
    switch (order.status) {
        case OrderStatus_Filled:
            if (!tradeIsOpen_) {
                return;
            }
            countLimitCloseFilled++;
            closePrice = (order.avgPrice > 0.0) ? order.avgPrice : currentTrade_.closeLimitOrder.price;
            closeQty = closePartiallyFilledQty +
                    ((order.cumExecQty > qtyEpsilon) ? order.cumExecQty : pendingCloseQty);
            if (order.cumExecQty > qtyEpsilon) {
                closeNotional += order.avgPrice * order.cumExecQty;
            }
            exitFee += order.cumExecFee;
            closeFillTimeMs = systemTimeMs();
            waitCloseLimitOrder_ = false;
            waitCancelCloseLimitOrder_ = false;
            finishTrade();
            break;
        case OrderStatus_Cancelled:
        case OrderStatus_Rejected:
        case OrderStatus_Expired:
        case OrderStatus_Deactivated: {
            if (!tradeIsOpen_) {
                return;
            }
            waitCloseLimitOrder_ = false;
            waitCancelCloseLimitOrder_ = false;

            closePartiallyFilledQty += order.cumExecQty;
            closeNotional += order.avgPrice * order.cumExecQty;
            exitFee += order.cumExecFee;
            const double remaining = openQty - closePartiallyFilledQty;
            if (remaining <= qtyEpsilon) {
                closePrice = (order.avgPrice > 0.0) ? order.avgPrice
                                                    : currentTrade_.closeLimitOrder.price;
                closeQty = closePartiallyFilledQty;
                closeFillTimeMs = systemTimeMs();
                finishTrade();
            } else {
                sendCloseMarket(remaining);
            }
            break;
        }
        default:
            break;
    }
}

void TradeManager::handleCloseMarketOrder(const OrderEvent &order) {
    switch (order.status) {
        case OrderStatus_Filled:
            countCloseMarket++;
            closeQty = closePartiallyFilledQty + order.cumExecQty;
            if (order.cumExecQty <= qtyEpsilon) {
                closeQty = closePartiallyFilledQty + pendingCloseQty;
            }
            closePrice = (order.avgPrice > 0.0) ? order.avgPrice : currentPrice;
            closeNotional += closePrice * order.cumExecQty;
            exitFee += order.cumExecFee;
            closeFillTimeMs = systemTimeMs();
            waitCloseMarketOrder_ = false;
            finishTrade();
            break;
        case OrderStatus_Cancelled:
        case OrderStatus_Rejected:
        case OrderStatus_Expired:
        case OrderStatus_Deactivated:
            waitCloseMarketOrder_ = false;
            std::cout << "TradeManager: close market order failed with status "
                      << orderStatusToString(order.status) << std::endl;
            if (order.cumExecQty > qtyEpsilon) {
                closePartiallyFilledQty += order.cumExecQty;
                closeNotional += order.avgPrice * order.cumExecQty;
                exitFee += order.cumExecFee;
            }
            //! Не закрываем сделку частично: следующая итерация checkCurrentPrice
            //! снова увидит SL/TP и повторит рыночное закрытие остатка.
            if (openQty - closePartiallyFilledQty <= qtyEpsilon) {
                closePrice = (order.avgPrice > 0.0) ? order.avgPrice : currentPrice;
                closeQty = closePartiallyFilledQty;
                closeFillTimeMs = systemTimeMs();
                finishTrade();
            }
            break;
        default:
            break;
    }
}

void TradeManager::enterTrade(const OrderEvent &order, bool byMarket) {
    Side side = currentTradeSide_.value_or(order.side);
    if (side == Side_Unknown) {
        side = order.side;
    }
    if (side == Side_Unknown) {
        std::cout << "TradeManager::enterTrade: unknown side" << std::endl;
        resetTradeState();
        currentTradeSide_ = std::nullopt;
        return;
    }

    currentTradeSide_ = side;
    tradeIsOpen_ = true;
    openByMarket_ = byMarket;
    startTrade_ = std::chrono::steady_clock::now();
    openFillTimeMs = systemTimeMs();
    openPrice = (order.avgPrice > 0.0) ? order.avgPrice
                                       : ((order.price > 0.0) ? order.price : currentPrice);
    openQty = (order.cumExecQty > qtyEpsilon)
                      ? order.cumExecQty
                      : ((order.qty > qtyEpsilon) ? order.qty : settings::defaultQty);
    entryFee = order.cumExecFee;

    waitOpenLimitOrder_ = false;
    waitOpenMarketOrder_ = false;
    waitCancelOpenLimitOrder_ = false;
    waitCloseLimitOrder_ = false;
    waitCloseMarketOrder_ = false;
    waitCancelCloseLimitOrder_ = false;

    currentTrade_.calcSLTP(openPrice, side);
}

void TradeManager::sendOpenMarketFallback() {
    if (!currentTradeSide_.has_value()) {
        return;
    }

    if (!currentTrade_.makeTradeByMarketOrder(currentTradeSide_.value())) {
        logOrderSent(currentTrade_.openMarketOrder, "open_market", false);
        std::cout << "TradeManager: can't send market open order" << std::endl;
        resetTradeState();
        currentTradeSide_ = std::nullopt;
        return;
    }

    waitOpenMarketOrder_ = true;
    timePlaceOpenMarketOrder_ = std::chrono::steady_clock::now();
    logOrderSent(currentTrade_.openMarketOrder, "open_market", true);
}

void TradeManager::sendCloseMarket(double qty) {
    if (!tradeIsOpen_ || !currentTradeSide_.has_value() || qty <= qtyEpsilon) {
        return;
    }

    const Side side = (currentTradeSide_ == Side_Buy) ? Side_Sell : Side_Buy;
    if (!currentTrade_.makeCloseMarketOrder(side, qty)) {
        logOrderSent(currentTrade_.closeMarketOrder, "close_market", false);
        std::cout << "TradeManager: can't send market close order" << std::endl;
        return;
    }

    pendingCloseQty = qty;
    waitCloseMarketOrder_ = true;
    timePlaceCloseOrder_ = std::chrono::steady_clock::now();
    logOrderSent(currentTrade_.closeMarketOrder, "close_market", true);
}

void TradeManager::finishTrade() {
    if (!currentTradeSide_.has_value()) {
        return;
    }

    const Side side = currentTradeSide_.value();
    if (closeQty > qtyEpsilon && closeNotional > 0.0) {
        closePrice = closeNotional / closeQty;
    }

    double grossPnl = (closePrice - openPrice) * closeQty;
    if (side == Side_Sell) {
        grossPnl *= -1.0;
    }

    const double fees = entryFee + exitFee;
    const double netPnl = grossPnl - fees;
    const double notional = openPrice * closeQty;
    const double netPnlPercent = (notional > qtyEpsilon) ? (netPnl / notional) * 100.0 : 0.0;

    totalProfit += netPnl;
    totalProfitPercent += netPnlPercent;
    totalCount++;
    if (netPnl > 0.0) {
        countPositive++;
    }
    if (closeReason_ == CloseReason_StopLoss) {
        stopLossCount++;
    } else if (closeReason_ == CloseReason_TakeProfit) {
        takeProfitCount++;
    } else if (closeReason_ == CloseReason_InverseSignal) {
        inverseSignalCount++;
    }

    logTradeResult(grossPnl, fees, netPnl, netPnlPercent);

    std::cout << "[TRADE CLOSED] #" << currentTrade_.currentTradeNumber << " "
              << sideToString(side) << " reason=" << closeReasonToString(closeReason_)
              << " open=" << openPrice << " close=" << closePrice << " qty=" << closeQty
              << " pnl=" << netPnl << " USDT (" << netPnlPercent << "%)"
              << " | total=" << totalProfit << " USDT over " << totalCount << " trades"
              << std::endl;

    waitCloseLimitOrder_ = false;
    waitCloseMarketOrder_ = false;
    waitCancelCloseLimitOrder_ = false;
    tradeIsOpen_ = false;
    currentTradeSide_ = std::nullopt;
}

void TradeManager::cancelOpenLimitOrder() {
    if (!waitOpenLimitOrder_) {
        return;
    }

    if (!currentTrade_.cancelOrder(currentTrade_.openLimitOrder.req_id)) {
        logOrderSent(currentTrade_.orderCancel, "cancel_open_limit", false);
        return;
    }

    waitOpenLimitOrder_ = false;
    waitCancelOpenLimitOrder_ = true;
    timePlaceOpenOrder_ = std::chrono::steady_clock::now();
    logOrderSent(currentTrade_.orderCancel, "cancel_open_limit", true);
}

void TradeManager::cancelCloseLimitOrder() {
    if (!waitCloseLimitOrder_) {
        return;
    }

    if (!currentTrade_.cancelOrder(currentTrade_.closeLimitOrder.req_id)) {
        logOrderSent(currentTrade_.orderCancel, "cancel_close_limit", false);
        return;
    }

    waitCloseLimitOrder_ = false;
    waitCancelCloseLimitOrder_ = true;
    timePlaceCloseOrder_ = std::chrono::steady_clock::now();
    logOrderSent(currentTrade_.orderCancel, "cancel_close_limit", true);
}

void TradeManager::checkSLTP() {
    if (!hasOpenTrade() || !tradeIsOpen_) {
        return;
    }

    const bool stopLoss =
            (currentTradeSide_ == Side_Sell && currentPrice >= currentTrade_.stopLossPrice) ||
            (currentTradeSide_ == Side_Buy && currentPrice <= currentTrade_.stopLossPrice);

    if (stopLoss) {
        closeReason_ = CloseReason_StopLoss;
        std::cout << "TradeManager: stop loss" << std::endl;
        sendCloseMarket(openQty - closePartiallyFilledQty);
        return;
    }

    const bool takeProfit =
            (currentTradeSide_ == Side_Sell && currentPrice <= currentTrade_.takeProfitPrice) ||
            (currentTradeSide_ == Side_Buy && currentPrice >= currentTrade_.takeProfitPrice);

    if (takeProfit) {
        //! Если часть позиции уже закрыта (например, частичный филл лимитного
        //! закрытия), добиваем остаток рынком, а не шлём новый лимит на весь объём.
        if (closePartiallyFilledQty > qtyEpsilon) {
            std::cout << "TradeManager: take profit, close remainder by market" << std::endl;
            sendCloseMarket(openQty - closePartiallyFilledQty);
            return;
        }
        std::cout << "TradeManager: take profit" << std::endl;
        closeTrade(CloseReason_TakeProfit);
    }
}

bool TradeManager::sideIsEqual(const Side &side) const {
    return currentTradeSide_.has_value() && currentTradeSide_.value() == side;
}

bool TradeManager::isCloseInProgress() const {
    return waitCloseLimitOrder_ || waitCloseMarketOrder_ || waitCancelCloseLimitOrder_;
}

bool TradeManager::isTerminalStatus(OrderStatus status) const {
    return status == OrderStatus_Filled || status == OrderStatus_Cancelled ||
            status == OrderStatus_Rejected || status == OrderStatus_Expired ||
            status == OrderStatus_Deactivated;
}

void TradeManager::moveOpenOrderToPrice() {
    if (!currentTradeSide_.has_value()) {
        return;
    }

    if (!currentTrade_.replaceLimitOpenOrder(
                CalcLimitPrice(currentPrice, currentTradeSide_.value()))) {
        std::cout << "TradeManager::moveOpenOrderToPrice: can't replace order" << std::endl;
    }
}

bool TradeManager::checkProfit() const {
    if (!currentTradeSide_.has_value()) {
        return false;
    }

    if (currentTradeSide_ == Side_Buy && currentPrice > openPrice) {
        return true;
    }
    if (currentTradeSide_ == Side_Sell && currentPrice < openPrice) {
        return true;
    }

    return false;
}

void TradeManager::resetTradeState() {
    tradeIsOpen_ = false;
    waitOpenLimitOrder_ = false;
    waitOpenMarketOrder_ = false;
    waitCancelOpenLimitOrder_ = false;
    waitCloseLimitOrder_ = false;
    waitCloseMarketOrder_ = false;
    waitCancelCloseLimitOrder_ = false;

    openPrice = 0.0;
    closePrice = 0.0;
    openQty = 0.0;
    closeQty = 0.0;
    pendingCloseQty = 0.0;
    closePartiallyFilledQty = 0.0;
    closeNotional = 0.0;
    entryFee = 0.0;
    exitFee = 0.0;
    openFillTimeMs = 0;
    closeFillTimeMs = 0;
    closeReason_ = CloseReason_None;
    openByMarket_ = false;
}

void TradeManager::logOrderSent(const OrderRequest &order, const char *role, bool success) {
    const uint64_t now = systemTimeMs();
    ordersLoger_.recordValue(now);
    ordersLoger_.recordValue(currentTrade_.currentTradeNumber);
    ordersLoger_.recordValue(success ? "SENT" : "SEND_FAILED");
    ordersLoger_.recordValue(role);
    ordersLoger_.recordValue(order.req_id);
    ordersLoger_.recordValue("");
    ordersLoger_.recordValue(sideToString(order.side));
    ordersLoger_.recordValue(orderTypeToString(order.order_type));
    ordersLoger_.recordValue(success ? "Sent" : "SendFailed");
    ordersLoger_.recordValue(order.price);
    ordersLoger_.recordValue(order.qty);
    ordersLoger_.recordValue(0.0);
    ordersLoger_.recordValue(0.0);
    ordersLoger_.recordValue(0.0);
    ordersLoger_.recordValue(0.0);
    ordersLoger_.recordValue(0);
    ordersLoger_.recordValue(now);
    ordersLoger_.recordValue(0);
    ordersLoger_.endStr();
}

void TradeManager::logOrderUpdate(const OrderEvent &order, Trade::OrderType orderType) {
    const uint64_t now = systemTimeMs();
    const uint64_t latencyUs = (order.recvTimeMs > 0 && order.updatedTime > 0 &&
            order.recvTimeMs >= order.updatedTime)
            ? (order.recvTimeMs - order.updatedTime) * 1000
            : 0;

    ordersLoger_.recordValue(now);
    ordersLoger_.recordValue(currentTrade_.currentTradeNumber);
    ordersLoger_.recordValue("UPDATE");
    ordersLoger_.recordValue(orderRoleToString(orderType));
    ordersLoger_.recordValue(order.orderLinkId);
    ordersLoger_.recordValue(order.orderId);
    ordersLoger_.recordValue(sideToString(order.side));
    ordersLoger_.recordValue(orderRoleToString(orderType));
    ordersLoger_.recordValue(orderStatusToString(order.status));
    ordersLoger_.recordValue(order.price);
    ordersLoger_.recordValue(order.qty);
    ordersLoger_.recordValue(order.avgPrice);
    ordersLoger_.recordValue(order.cumExecQty);
    ordersLoger_.recordValue(order.leavesQty);
    ordersLoger_.recordValue(order.cumExecFee);
    ordersLoger_.recordValue(order.updatedTime);
    ordersLoger_.recordValue(order.recvTimeMs);
    ordersLoger_.recordValue(latencyUs);
    ordersLoger_.endStr();
}

void TradeManager::logTradeResult(double grossPnl, double fees, double netPnl,
        double netPnlPercent) {
    const Side side = currentTradeSide_.value_or(Side_Unknown);
    const uint64_t holdMs = (closeFillTimeMs >= openFillTimeMs && openFillTimeMs > 0)
            ? (closeFillTimeMs - openFillTimeMs)
            : 0;

    tradesLoger_.recordValue(currentTrade_.currentTradeNumber);
    tradesLoger_.recordValue(sideToString(side));
    tradesLoger_.recordValue(openFillTimeMs);
    tradesLoger_.recordValue(closeFillTimeMs);
    tradesLoger_.recordValue(holdMs);
    tradesLoger_.recordValue(openByMarket_ ? "market" : "limit");
    tradesLoger_.recordValue(closeReasonToString(closeReason_));
    tradesLoger_.recordValue(openPrice);
    tradesLoger_.recordValue(closePrice);
    tradesLoger_.recordValue(closeQty);
    tradesLoger_.recordValue(openPrice * closeQty);
    tradesLoger_.recordValue(grossPnl);
    tradesLoger_.recordValue(fees);
    tradesLoger_.recordValue(netPnl);
    tradesLoger_.recordValue(netPnlPercent);
    tradesLoger_.recordValue(totalProfit);
    tradesLoger_.endStr();
}
