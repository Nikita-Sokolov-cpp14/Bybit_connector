#include "trade.h"

#include <chrono>
#include <string>
#include <cstring>

#include "settings.h"

namespace {

const uint32_t idOpenOrder = 1;
const uint32_t idStopLoss = 2;
const uint32_t idTakeProfit = 3;
const uint32_t idCloseLimitOrder = 4;
const uint32_t idCloseMarketOrder = 5;

} // namespace

Trade::Trade() :
orderSender(),
tradeNumber(22325),
currentTradeNumber(0),
takeProfitPrice(0.0),
stopLossPrice(0.0),
orderIsPlaced(false),
orderIsFilled(false) {
    // orderOpenTrade.typeOrderRequest = TypeOrderRequest_New;
    // //! TODO: Подумать - открывать рыночным или лимитным
    // // orderOpenTrade.order_type = OrderType_Limit;
    // orderOpenTrade.order_type = OrderType_Market;
    // orderOpenTrade.qty = settings::defaultQty;
    // //! TODO: Для лимитного ордера нужно указать цену чуть ниже
    // // orderOpenTrade.price = price +- ;
    // orderOpenTrade.leverage = settings::leverage;
    // strcpy(orderOpenTrade.symbol, "BTCUSDT");

    stopLoss.typeOrderRequest = TypeOrderRequest_New;
    stopLoss.order_type = OrderType_Market;
    stopLoss.qty = settings::defaultQty;
    stopLoss.leverage = settings::leverage;
    stopLoss.closeOnTrigger = true;
    strcpy(stopLoss.symbol, "BTCUSDT");

    takeProfit.typeOrderRequest = TypeOrderRequest_New;
    takeProfit.order_type = OrderType_Limit;
    takeProfit.qty = settings::defaultQty;
    takeProfit.leverage = settings::leverage;
    strcpy(takeProfit.symbol, "BTCUSDT");

    openLimitOrder.typeOrderRequest = TypeOrderRequest_New;
    openLimitOrder.order_type = OrderType_Limit;
    openLimitOrder.qty = settings::defaultQty;
    openLimitOrder.leverage = settings::leverage;
    strcpy(openLimitOrder.symbol, "BTCUSDT");

    closeLimitOrder.typeOrderRequest = TypeOrderRequest_New;
    closeLimitOrder.order_type = OrderType_Limit;
    closeLimitOrder.qty = settings::defaultQty;
    closeLimitOrder.leverage = settings::leverage;
    strcpy(closeLimitOrder.symbol, "BTCUSDT");

    orderCancel.typeOrderRequest = TypeOrderRequest_Cancel;
    strcpy(orderCancel.symbol, "BTCUSDT");
    orderCancel.typeOrderId_ = TypeOrderId_OrderLinkId;

    closeMarketOrder.typeOrderRequest = TypeOrderRequest_New;
    closeMarketOrder.order_type = OrderType_Market;
    closeMarketOrder.qty = settings::defaultQty;
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
    openLimitOrder.req_id = currentTradeNumber * 10 + idOpenOrder; // Основной ордер +1
    openLimitOrder.side = side;
    openLimitOrder.price = price;
    openLimitOrder.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                          .count();
    ordersStatus[openLimitOrder.req_id] = OrderStatus_Unknown;

    return sendOrder(openLimitOrder);
}

bool Trade::makeTPSLOrders(const double price, const Side &side) {
    calcSLTP(price, side);

    stopLoss.req_id = currentTradeNumber * 10 + idStopLoss; //! TODO: +2 - это стоп
    stopLoss.triggerPrice = stopLossPrice;
    stopLoss.triggerSide = (side == Side_Buy) ? Side_Sell : Side_Buy;
    stopLoss.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                    .count();
    ordersStatus[stopLoss.req_id] = OrderStatus_Unknown;

    takeProfit.req_id = currentTradeNumber * 10 + idTakeProfit; //! TODO: +3 - это тейк
    takeProfit.side = (side == Side_Buy) ? Side_Sell : Side_Buy;
    takeProfit.price = takeProfitPrice;
    takeProfit.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                      .count();
    ordersStatus[takeProfit.req_id] = OrderStatus_Unknown;

    return (sendOrder(stopLoss) && sendOrder(takeProfit));
}

bool Trade::makeCloseLimitOrder(const double price, const Side &side) {
    closeLimitOrder.req_id = currentTradeNumber * 10 + idCloseLimitOrder;
    closeLimitOrder.side = side;
    closeLimitOrder.price = price;
    closeLimitOrder.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                           .count();
    ordersStatus[closeLimitOrder.req_id] = OrderStatus_Unknown;

    return sendOrder(closeLimitOrder);
}

bool Trade::makeCloseMarketOrder(const Side &side) {
    closeMarketOrder.req_id = currentTradeNumber * 10 + idCloseMarketOrder;
    closeMarketOrder.side = side;
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
    openLimitOrder.price = price;

    return sendOrder(openLimitOrder);
}
