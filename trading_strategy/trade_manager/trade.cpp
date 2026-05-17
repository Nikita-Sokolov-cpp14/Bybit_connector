#include "trade.h"

#include <chrono>
#include <string>
#include <cstring>

#include "settings.h"

Trade::Trade() :
tradeNumber(0),
takeProfitPrice(0.0),
stopLossPrice(0.0) {
    orderOpenTrade.typeOrderRequest = TypeOrderRequest_New;
    //! TODO: Подумать - открывать рыночным или лимитным
    // orderOpenTrade.order_type = OrderType_Limit;
    orderOpenTrade.order_type = OrderType_Market;
    orderOpenTrade.qty = settings::defaultQty;
    //! TODO: Для лимитного ордера нужно указать цену чуть ниже
    // orderOpenTrade.price = price +- ;
    orderOpenTrade.leverage = settings::leverage;
    strcpy(orderOpenTrade.symbol, "BTCUSDT");

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
}

void Trade::makeTrade(double price, const Side &side) {
    switch (side) {
        case Side_Buy:
            takeProfitPrice = price + settings::coefTakeProfit * price;
            stopLossPrice = price - settings::coefStopLoss * price;
            std::cout << "Trade::makeTrade: Buy. price = " << price << " ";
            break;
        case Side_Sell:
            takeProfitPrice = price - settings::coefTakeProfit * price;
            stopLossPrice = price + settings::coefStopLoss * price;
            std::cout << "Trade::makeTrade: Sell. price = " << price << " ";
            break;
        default:
            std::cout << "Trade::makeTrade: Unknown side" << std::endl;
            break;
    }
    std::cout << "take profit = " << takeProfitPrice << " ";
    std::cout << "stop loss = " << stopLossPrice << std::endl;

    orderOpenTrade.req_id = tradeNumber * 10 + 1; //! TODO: +1 - это открытие сделки
    orderOpenTrade.side = side;
    orderOpenTrade.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                          .count();

    stopLoss.req_id = tradeNumber * 10 + 2; //! TODO: +2 - это стоп
    stopLoss.triggerPrice = stopLossPrice;
    stopLoss.triggerSide = (side == Side_Buy) ? Side_Sell : Side_Buy;
    stopLoss.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                    .count();

    takeProfit.req_id = tradeNumber * 10 + 3; //! TODO: +3 - это тейк
    takeProfit.side = (side == Side_Buy) ? Side_Sell : Side_Buy;
    takeProfit.price = takeProfitPrice;
    takeProfit.enqueue_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                          .count();
}
