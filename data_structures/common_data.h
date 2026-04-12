#pragma once

#include <cstdint>
#include <string_view>

enum Side {
    Side_Unknown = 0,
    Side_Buy,
    Side_Sell
};

enum Category {
    Category_Unknown = 0,
    Category_Linear,    // "linear"
    Category_Inverse,   // "inverse"
    Category_Option     // "option"
};

enum OrderType {
    OrderType_Unknown = 0,
    OrderType_Market,
    OrderType_Limit,
    OrderType_StopMarket,
    OrderType_StopLimit,
    OrderType_TakeProfitMarket,
    OrderType_TakeProfitLimit,
    OrderType_StopLossMarket,
    OrderType_StopLossLimit
};
