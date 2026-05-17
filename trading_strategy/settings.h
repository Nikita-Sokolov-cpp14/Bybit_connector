#include <cstdint>
#include <iostream>
#include <chrono>

namespace settings {

//!< Глубина стакана для расчета дисбаланса.
static const size_t disbalanceDepthCalc = 20;

//!< Размер истории средней цены.
static const size_t historyMidPriceSize = 50;
static const size_t historyPublicTradeSize = 50;

static const double buyDisbalance = 2.0;
static const double sellDisbalance = 0.5;

static const uint8_t leverage = 50;
// Объем
static const double defaultQty = 0.001; // TODO: Пока для BTC

static const double coefTakeProfit = 0.15 / 100.0; // В долях, а не процентах. 1%
static const double coefStopLoss = 0.15 / 100.0; // В долях, а не процентах. 1%

static const std::chrono::milliseconds tradeTimeOut = std::chrono::milliseconds(20 * 1000);

}
