#pragma once

#include <cstdint>
#include <iostream>
#include <functional>
#include <optional>
#include <string>

#include "data_structures/order_request.h"
#include "data_structures/order.h"
#include "data_structures/position.h"
#include "loger/loger.h"
#include "trade.h"

/**
 * @brief Снимок обновления ордера, который передаётся из IO-потока в поток стратегии.
 * @details Все строковые поля являются владеющими: исходный message_ перезаписывается
 * сразу после разбора, поэтому хранить string_view из OrderHFT нельзя.
 */
struct OrderEvent {
    std::string orderLinkId;
    std::string orderId;
    Side side = Side_Unknown;
    OrderStatus status = OrderStatus_Unknown;
    double price = 0.0;
    double qty = 0.0;
    double avgPrice = 0.0;
    double cumExecQty = 0.0;
    double cumExecFee = 0.0;
    double leavesQty = 0.0;
    uint64_t updatedTime = 0;
    uint64_t recvTimeMs = 0;
};

enum CloseReason {
    CloseReason_None = 0,
    CloseReason_TakeProfit,
    CloseReason_StopLoss,
    CloseReason_Timeout,
    CloseReason_InverseSignal,
    CloseReason_Manual
};

class TradeManager {
public:
    TradeManager();

    void setOrderSender(Trade::OrderSender orderSender);
    void setOrder(const OrderEvent &order);
    void onOrderRequestResult(const OrderRequestResult &result);
    void setPosition(const PositionHFT &position);

    void makeTrade(const double price, const Side &side);

    bool hasOpenBuyTrade() const;
    bool hasOpenSellTrade() const;

    void closeTrade(CloseReason reason = CloseReason_Manual);

    // Проверить - не противоположный ли сигнал?
    void checkInverseSignal(const Side &side);

    bool hasOpenTrade() const;

    void checkCurrentPrice(const double price);

private:
    std::optional<Side> currentTradeSide_;
    Trade currentTrade_;

    double openPrice = 0.0;
    double closePrice = 0.0;
    double currentPrice = 0.0;
    double openQty = 0.0;
    double closeQty = 0.0;
    double pendingCloseQty = 0.0;
    double closePartiallyFilledQty = 0.0;
    double closeNotional = 0.0;
    double entryFee = 0.0;
    double exitFee = 0.0;

    uint64_t openFillTimeMs = 0;
    uint64_t closeFillTimeMs = 0;

    std::chrono::steady_clock::time_point startTrade_;
    std::chrono::steady_clock::time_point timePlaceOpenOrder_;
    std::chrono::steady_clock::time_point timePlaceOpenMarketOrder_;
    std::chrono::steady_clock::time_point timePlaceCloseOrder_;

    bool waitOpenLimitOrder_ = false;
    bool waitOpenMarketOrder_ = false;
    bool waitCancelOpenLimitOrder_ = false;
    bool waitCloseLimitOrder_ = false;
    bool waitCloseMarketOrder_ = false;
    bool waitCancelCloseLimitOrder_ = false;
    bool tradeIsOpen_ = false;

    CloseReason closeReason_ = CloseReason_None;
    bool openByMarket_ = false;

    Logger ordersLoger_;
    Logger tradesLoger_;

    void handleOpenLimitOrder(const OrderEvent &order);
    void handleOpenMarketOrder(const OrderEvent &order);
    void handleCloseLimitOrder(const OrderEvent &order);
    void handleCloseMarketOrder(const OrderEvent &order);

    void enterTrade(const OrderEvent &order, bool byMarket);
    void sendOpenMarketFallback();
    void sendCloseMarket(double qty);

    void checkSLTP();
    void retryClose();

    void finishTrade();

    void cancelOpenLimitOrder();
    void cancelCloseLimitOrder();

    bool sideIsEqual(const Side &side) const;
    bool isCloseInProgress() const;
    bool isTerminalStatus(OrderStatus status) const;

    void moveOpenOrderToPrice();

    bool checkProfit() const;

    void resetTradeState();

    void logOrderSent(const OrderRequest &order, const char *role, bool success);
    void logOrderUpdate(const OrderEvent &order, Trade::OrderType orderType);
    void logTradeResult(double grossPnl, double fees, double netPnl, double netPnlPercent);
};
