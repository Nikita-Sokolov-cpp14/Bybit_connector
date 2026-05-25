#include <cstdint>
#include <iostream>
#include <chrono>

namespace settings {

//!< Глубина стакана для расчета дисбаланса.
static const size_t disbalanceDepthCalc = 30;

//!< Размер истории средней цены.
static const size_t historyMidPriceSize = 50;
static const size_t historyPublicTradeSize = 50;

static const double buyDisbalance = 2.0;
static const double sellDisbalance = 0.5;

static const double inverseBuyDisbalance = 4.0;
static const double inverseSellDisbalance = 0.25;
static const size_t countInverseSignal = 1;
static const size_t countSignal = 1;

static const size_t averrageDisbalanceCount = 10;

static const uint8_t leverage = 50;
// Объем
static const double defaultQty = 0.001; // TODO: Пока для BTC

static const double coefTakeProfit = 0.15 / 100.0; // В долях, а не процентах. 1%
static const double coefStopLoss = 0.15 / 100.0; // В долях, а не процентах. 1%

// Отступ лимитной цены для лимитного ордера при входе в сделку
static const double spaceToLimitPrice = 0.0007 / 100.0;

static const std::chrono::milliseconds tradeTimeOut = std::chrono::milliseconds(60 * 1000);
static const std::chrono::milliseconds waitOpenLimitOrderTime = std::chrono::milliseconds(5000);
static const std::chrono::milliseconds waitCloseLimitOrderTime = std::chrono::milliseconds(5000);

}
