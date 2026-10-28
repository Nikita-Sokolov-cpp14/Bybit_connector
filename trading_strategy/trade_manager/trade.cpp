#include "trade.h"

#include <chrono>
#include <string>
#include <cstring>

#include "settings.h"

Trade::Trade() :
orderSender(),
tradeNumber(25010),
currentTradeNumber(0),
takeProfitPrice(0.0),
stopLossPrice(0.0),
orderIsPlaced(false),
orderIsFilled(false) {
    openLimitOrder.typeOrderRequest = TypeOrderRequest_New;
    openLimitOrder.order_type = OrderType_Limit;
    openLimitOrder.qty = settings::defaultQty;
    openLimitOrder.leverage = settings::leverage;
    strcpy(openLimitOrder.symbol, "BTCUSDT");

    openMarketOrder.typeOrderRequest = TypeOrderRequest_New;
    openMarketOrder.order_type = OrderType_Market;
    openMarketOrder.qty = settings::defaultQty;
    openMarketOrder.leverage = settings::leverage;
    strcpy(openMarketOrder.symbol, "BTCUSDT");

    closeLimitOrder.typeOrderRequest = TypeOrderRequest_New;
    closeLimitOrder.order_type = OrderType_Limit;
    closeLimitOrder.qty = settings::defaultQty;
    closeLimitOrder.reduceOnly = true;
    closeLimitOrder.leverage = settings::leverage;
    strcpy(closeLimitOrder.symbol, "BTCUSDT");

    orderCancel.typeOrderRequest = TypeOrderRequest_Cancel;
    strcpy(orderCancel.symbol, "BTCUSDT");
    orderCancel.typeOrderId_ = TypeOrderId_OrderLinkId;

    closeMarketOrder.typeOrderRequest = TypeOrderRequest_New;
    closeMarketOrder.order_type = OrderType_Market;
    closeMarketOrder.qty = settings::defaultQty;
    closeMarketOrder.reduceOnly = true;
    closeMarketOrder.leverage = settings::leverage;
    strcpy(closeMarketOrder.symbol, "BTCUSDT");
}

void Trade::clearStatuses() {
    ordersStatus.clear();
}

bool Trade::makeTradeByLimitOrder(const double price, const Side &side) {
    currentTradeNumber = tradeNumber;
    tradeNumber++;
    ordersStatus.clear();

    openLimitOrder.typeOrderRequest = TypeOrderRequest_New;
    openLimitOrder.typeOrderId_ = TypeOrderId_OrderLinkId;
    openLimitOrder.req_id = makeReqId(OrderType_OpenLimitOrder); // Открывающий лимитный ордер +1
    openLimitOrder.side = side;
    openLimitOrder.price = price;
    openLimitOrder.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                          .count();
    ordersStatus[openLimitOrder.req_id] = OrderStatus_Unknown;

    return sendOrder(openLimitOrder);
}

bool Trade::makeTradeByMarketOrder(const Side &side) {
    //!< NOTE: Рыночный ордер отправляется, если лимитный был неудачным.
    // текущая сделка с точки зрения ordersStatus продолжается.

    openMarketOrder.typeOrderRequest = TypeOrderRequest_New;
    openMarketOrder.typeOrderId_ = TypeOrderId_OrderLinkId;
    openMarketOrder.req_id = makeReqId(OrderType_OpenMarketOrder); // Открывающий рыночный ордер +1
    openMarketOrder.side = side;
    openMarketOrder.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                           .count();
    ordersStatus[openMarketOrder.req_id] = OrderStatus_Unknown;

    return sendOrder(openMarketOrder);
}

bool Trade::makeCloseLimitOrder(const double price, const Side &side, double qty) {
    closeLimitOrder.typeOrderRequest = TypeOrderRequest_New;
    closeLimitOrder.typeOrderId_ = TypeOrderId_OrderLinkId;
    closeLimitOrder.req_id = makeReqId(OrderType_CloseLimitOrder);
    closeLimitOrder.side = side;
    closeLimitOrder.price = price;
    closeLimitOrder.qty = qty;
    closeLimitOrder.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                           .count();
    ordersStatus[closeLimitOrder.req_id] = OrderStatus_Unknown;

    return sendOrder(closeLimitOrder);
}

bool Trade::makeCloseMarketOrder(const Side &side, double qty) {
    closeMarketOrder.typeOrderRequest = TypeOrderRequest_New;
    closeMarketOrder.typeOrderId_ = TypeOrderId_OrderLinkId;
    closeMarketOrder.req_id = makeReqId(OrderType_CloseMarketOrder);
    closeMarketOrder.side = side;
    closeMarketOrder.qty = qty;
    closeMarketOrder.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
    ordersStatus[closeMarketOrder.req_id] = OrderStatus_Unknown;

    return sendOrder(closeMarketOrder);
}

void Trade::calcSLTP(double price, const Side &side) {
    switch (side) {
        case Side_Buy:
            takeProfitPrice = price + settings::coefTakeProfit * price;
            stopLossPrice = price - settings::coefStopLoss * price;
            break;
        case Side_Sell:
            takeProfitPrice = price - settings::coefTakeProfit * price;
            stopLossPrice = price + settings::coefStopLoss * price;
            break;
        default:
            std::cout << "Trade::makeTrade: Unknown side" << std::endl;
            break;
    }

    // std::cout << "take profit = " << takeProfitPrice << " ";
    // std::cout << "stop loss = " << stopLossPrice << std::endl;
}

bool Trade::checkOrderStatus() {
    return false;
}

bool Trade::cancelOrder(const uint64_t orderId) {
    orderCancel.typeOrderRequest = TypeOrderRequest_Cancel;
    orderCancel.typeOrderId_ = TypeOrderId_OrderLinkId;
    orderCancel.req_id = orderId;
    orderCancel.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
    std::snprintf(orderCancel.order_link_id, sizeof(orderCancel.order_link_id), "%llu",
            static_cast<unsigned long long>(orderId));

    return sendOrder(orderCancel);
}

bool Trade::sendOrder(const OrderRequest &order) {
    if (!orderSender) {
        std::cout << "Trade::sendOrder: orderSender is undefined" << std::endl;
        return false;
    }

    if (!orderSender(order)) {
        std::cout << "Trade::sendOrder: can't place order" << std::endl;
        return false;
    }

    return true;
}

bool Trade::replaceLimitOpenOrder(double price) {
    openLimitOrder.typeOrderRequest = TypeOrderRequest_Replace;
    openLimitOrder.typeOrderId_ = TypeOrderId_OrderLinkId;
    openLimitOrder.price = price;
    std::snprintf(openLimitOrder.order_link_id, sizeof(openLimitOrder.order_link_id), "%llu",
            static_cast<unsigned long long>(openLimitOrder.req_id));

    return sendOrder(openLimitOrder);
}

Trade::OrderType Trade::getOrderType(const uint64_t orderLinkId) {
    uint64_t type = orderLinkId % multiplier;

    switch (type) {
        case 1:
            return OrderType_OpenLimitOrder;
        case 2:
            return OrderType_OpenMarketOrder;
        case 3:
            return OrderType_CloseLimitOrder;
        case 4:
            return OrderType_CloseMarketOrder;
        default:
            break;
    }

    return OrderType_Unknown;
}

uint64_t Trade::makeReqId(const OrderType &orderType) {
    uint64_t id = 0;

    switch (orderType) {
        case OrderType_OpenLimitOrder:
            id = 1;
            break;
        case OrderType_OpenMarketOrder:
            id = 2;
            break;
        case OrderType_CloseLimitOrder:
            id = 3;
            break;
        case OrderType_CloseMarketOrder:
            id = 4;
            break;
        default:
            break;
    }

    return currentTradeNumber * multiplier + id;
}
