#include <cstdint>
#include <iostream>
#include <chrono>

namespace settings {

//!< Глубина стакана для расчета дисбаланса.
static const size_t disbalanceDepthCalc = 20;

//!< Размер истории средней цены.
static const size_t historyMidPriceSize = 50;
static const size_t historyPublicTradeSize = 50;

static const double countSigmaBuy = 1.2;
static const double countSigmaSell = -1.2;

static const double inverseBuyDisbalance = 4.0;
static const double inverseSellDisbalance = 0.25;
static const size_t countInverseSignal = 1;
static const size_t countSignal = 1;

// OBI

//! Число тиков для накопления агрегированного дисбаланса. 1 тик = 20 мс по умолчанию.
static const size_t agregateCountObi = 50;
//! Размер данных с истории. 1 ячейка = 1 массив агрегированных данных за выбранное время.
static const size_t averrageAgrCount = 7200;
//! Размер окна prev агрегированных данных.
static const size_t obiAgrWindowSizePrev = 2700;

// Объем
static const double defaultQty = 0.001; // TODO: Пока для BTC

//! ============================================================================
//! PFD intraday-to-swing: основной вариант из offline walk-forward.
//! TP 2.0%, SL 0.5%, максимальное удержание 24 часа, без trailing-stop.
//!
//! Более консервативный вариант:
//!   coefTakeProfit = 1.5 / 100.0;   // TP 1.5%
//!   coefStopLoss   = 0.5 / 100.0;   // SL 0.5%
//!   tradeTimeOut   = 24h (не меняется)
//! У него mean net PnL ниже, но median net выше и профиль мягче:
//! меньше зависимость от нескольких крупных движений.
//! ============================================================================
static const double coefTakeProfit = 2.0 / 100.0; // В долях. 2.0%
static const double coefStopLoss = 0.5 / 100.0; // В долях. 0.5%

//! 24 часа. Раньше было 3300 с.
static const std::chrono::milliseconds tradeTimeOut = std::chrono::milliseconds(24 * 3600 * 1000);
static const uint8_t leverage = 50;

// TFI
static const std::chrono::milliseconds shortTime = std::chrono::milliseconds(180 * 1000);
static const std::chrono::milliseconds baseTime = std::chrono::milliseconds(900 * 1000);

static const double tradeFlowCountSigmaBuy = 0.65;
static const double tradeFlowCountSigmaSell = -0.65;

//! PFD (Price-Flow Divergence), comb = pz - intz.
//! Параметры соответствуют лучшему offline-варианту 6-24h:
//!   interval=60s, W=120, sigma=3.0, confirm=1,
//!   vol_ratio >= 0.8 (rolling median 720 интервалов),
//!   direction=both, intz_weight=1.0, TP=2%, SL=0.5%, hold=24h.
//!
//! ВАЖНО: offline-исследование использовало price_source=vwap
//! (VWAP последней секунды интервала). В C++ индикатор получает mid-price
//! из стакана через setMidPrice(), поэтому live-сигналы могут отличаться.
//! Для точного совпадения с research нужен отдельный источник last-second VWAP.

//! Шаг выборки индикатора.
static const std::chrono::milliseconds pfdIntervalStep = std::chrono::milliseconds(60 * 1000);
//! Число интервалов в окне W индикатора. 60 с * 120 = 2 часа.
static const size_t pfdWindowSize = 120;
//! Порог на comb (безразмерный комбо-сигнал).
//! Buy (моментум): цена обгоняет поток -> comb >= +pfdSigma.
//! Sell (моментум): цена отстаёт от потока -> comb <= -pfdSigma.
static const double pfdSigma = 3.0;

//! Число предыдущих интервалов, на которых должен подтвердиться тот же
//! сырой сигнал. 0 = без подтверждения, 1 = как в лучшем offline-варианте.
static const size_t pfdConfirmIntervals = 1;

//! Режимный фильтр волатильности: текущий sigPx должен быть в диапазоне
//! [pfdVolRatioMin, pfdVolRatioMax] от rolling-медианы sigPx за
//! pfdVolWindowSize интервалов. 0 = фильтр выключен.
//! В лучшем offline-варианте: window=720 (12ч), min=0.8, max=без ограничения.
static const size_t pfdVolWindowSize = 720;
static const double pfdVolRatioMin = 0.8;
static const double pfdVolRatioMax = 1e9;
//! Warmup: W+1 интервалов для PFD + pfdVolWindowSize валидных sigPx,
//! т.е. около 14 часов при interval=60s. До заполнения окна сигналов нет.
//! Offline-исследование включало нули warmup-периода в rolling-медиану;
//! в C++ используются только валидные sigPx, поэтому старт чуть позже.

} // namespace settings
