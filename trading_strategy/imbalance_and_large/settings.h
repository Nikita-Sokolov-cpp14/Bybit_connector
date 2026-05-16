#include <cstdint>
#include <iostream>

namespace settings {

//!< Глубина стакана для расчета дисбаланса.
static const size_t disbalanceDepthCalc = 20;

//!< Размер истории средней цены.
static const size_t historyMidPriceSize = 50;
static const size_t historyPublicTradeSize = 50;

static const double buyDisbalance = 2.0;
static const double sellDisbalance = 0.5;

}
