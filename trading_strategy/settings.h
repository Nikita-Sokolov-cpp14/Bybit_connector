#include <cstdint>
#include <iostream>
#include <chrono>

namespace settings {

//!< Глубина стакана для расчета дисбаланса.
static const size_t disbalanceDepthCalc = 50;

//!< Размер истории средней цены.
static const size_t historyMidPriceSize = 50;
static const size_t historyPublicTradeSize = 50;

static const double countSigmaBuy = 1.88;
static const double countSigmaSell = -1.88;

static const double inverseBuyDisbalance = 4.0;
static const double inverseSellDisbalance = 0.25;
static const size_t countInverseSignal = 1;
static const size_t countSignal = 1;

static const size_t averrageDisbalanceCount = 150;
static const size_t obiWindowSizeRecent = 75;
static const size_t obiWindowSizePrev = 75;
static const double minShift = 0.15;

static const uint8_t leverage = 50;
// Объем
static const double defaultQty = 0.001; // TODO: Пока для BTC

static const double coefTakeProfit = 0.15 / 100.0; // В долях, а не процентах. 1%
static const double coefStopLoss = 0.08 / 100.0; // В долях, а не процентах. 1%

static const std::chrono::milliseconds tradeTimeOut = std::chrono::milliseconds(180 * 1000);

}
