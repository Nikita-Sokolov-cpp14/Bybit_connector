#include <cstdint>
#include <iostream>
#include <chrono>

namespace settings {

//!< Глубина стакана для расчета дисбаланса.
static const size_t disbalanceDepthCalc = 50;

//!< Размер истории средней цены.
static const size_t historyMidPriceSize = 50;
static const size_t historyPublicTradeSize = 50;

static const double countSigmaBuy = 0.75;
static const double countSigmaSell = -0.75;

static const double inverseBuyDisbalance = 4.0;
static const double inverseSellDisbalance = 0.25;
static const size_t countInverseSignal = 1;
static const size_t countSignal = 1;

// OBI

//! Число тиков для накопления агрегированного дисбаланса. 1 тик = 20 мс по умолчанию.
static const size_t agregateCountObi = 10;
//! Размер данных с истории. 1 ячейка = 1 массив агрегированных данных за выбранное время.
static const size_t averrageAgrCount = 3600;
//! Размер окна prev агрегированных данных.
static const size_t obiAgrWindowSizePrev = 240;
static const double minShift = 0.15;

// Объем
static const double defaultQty = 0.001; // TODO: Пока для BTC

static const double coefTakeProfit = 0.25 / 100.0; // В долях, а не процентах. 1%
static const double coefStopLoss = 0.10 / 100.0; // В долях, а не процентах. 1%

static const std::chrono::milliseconds tradeTimeOut = std::chrono::milliseconds(3300 * 1000);
static const uint8_t leverage = 50;

// TFI
static const std::chrono::milliseconds shortTime = std::chrono::milliseconds(5 * 1000);
static const std::chrono::milliseconds baseTime = std::chrono::milliseconds(60 * 1000);

static const double tradeFlowCountSigmaBuy = 2.3;
static const double tradeFlowCountSigmaSell = -2.3;

}
