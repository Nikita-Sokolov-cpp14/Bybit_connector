# PFD Hyperparameter Optimizer

Offline grid search + walk-forward for **Price-Flow Divergence (PFD)** on Bybit
1-second aggregated data.

## Input

`research_on_data/aggregated_1s/aggregated_1s.csv`

Columns: `ts, open, high, low, close, buy_vol, sell_vol, trade_count, vwap`
- `ts` — UTC ms (start of second), int64
- prices/volumes float, `trade_count` int, `vwap` float

The loader detects tab/comma/semicolon, sorts by `ts`, removes duplicate
timestamps and drops unusable rows.

## Install

```bash
cd research_on_data/pfd_optimizer
pip install -r requirements.txt
```

## Run

```bash
python run_optimization.py \
  --config config.yaml \
  --data ../aggregated_1s/aggregated_1s.csv \
  --output output
```

Quick checks:
- `config_micro.yaml` — one configuration, smoke test.
- `config_small.yaml` — reduced grid.
- `config_accuracy.yaml` — direction/accuracy research grid (see below).

Directional-accuracy research without the full backtest:

```bash
python research_pfd.py --data ../aggregated_1s/aggregated_1s.csv   --interval 60 --window 360 --horizons 120,240,360   --intz-weights 0,0.5,1.0 --trend-emas 0,120,360 --out output/research
```

It writes `output/research/direction_accuracy.csv` with long/short accuracy,
mean net return after fees, and accuracy per chronological block (a cheap
stability check).

Early-exit research:

```bash
python research_exit.py --data ../aggregated_1s/aggregated_1s.csv \
  --thresholds none,0,0.5,1,1.5,2,2.5,3 --out output/research
```

It exits a position when the same `comb` score turns opposite (long:
`comb <= -T`, short: `comb >= +T`) and writes
`output/research/exit_test_summary.csv`. Result on the best 6-24h config:
**the opposite-PFD exit does not improve the strategy** - it cuts winners and
reduces net PnL/PF. Baseline: +0.383% mean PnL/trade, PF 2.13, +1.684%
mean net/fold. Best opposite-exit variant (T=3.0): +0.311%, PF 1.91, +1.493%.

Trailing-stop research:

```bash
python research_trailing.py --data ../aggregated_1s/aggregated_1s.csv \
  --out output/research
```

It starts trailing after `activation` profit and trails the best high/low by
`distance`, keeping the initial 0.5% stop before activation. Result: trailing
does **not** improve net PnL. A tight 0.5% trail after +0.25% raises win rate
to 60.7% and median Sharpe to 0.94 (baseline 0.51) and slightly lowers max DD,
but mean net/fold falls from +1.684% to +1.344%. Loose trails are equivalent
to the baseline. Details are in `PFD_REPORT.txt` section 4.9.

## PFD logic (matches C++)

For each `interval_sec` bucket:
1. `buyAcc=sum(buy_vol)`, `sellAcc=sum(sell_vol)`,
   `flow=(buyAcc-sellAcc)/(buyAcc+sellAcc)` (0 if total < 1e-12).
2. `price_mid` = last second `vwap` or `close` (`price_source`).
3. Buffers `price_series`, `flow_series` have length `W+1`; no signal until full.
4. `tr = p[i]/p[i-W]-1`.
5. `rets = p[j]/p[j-1]-1`, `j=i-W+1..i`,
   `sigPx = std(rets, ddof=1) * sqrt(W)`.
6. `flow_window = flow[i-W:i]` (last **W**, not W+1),
   `intz = mean(flow_window) * sqrt(W) / std(flow_window, ddof=1)`.
7. `pz = tr / sigPx`, `comb = pz - intz`.
   Guard: `sigPx < 1e-9` or `stdFlow < 1e-9` -> neutral.
8. `comb >= sigma` -> Buy, `comb <= -sigma` -> Sell.

The implementation is fully vectorised (pandas rolling + NumPy) and is checked
against the C++/naive loop on synthetic data.

### Research extensions (optional, default = original spec)

Additional grid keys can be added to `params`:

| Key | Default | Meaning |
|---|---|---|
| `signal_mode` | `momentum` | `momentum` or `reversal` (opposite sign) |
| `confirm_intervals` | `0` | require N previous signals of the same side |
| `trend_ema_span` | `0` | EMA span in intervals; 0 = off |
| `trend_filter_mode` | `aligned` | `aligned` (Buy above EMA) or `against` |
| `direction` | `both` | `both`, `long`, `short` |
| `pz_weight` | `1.0` | weight of `pz` in the score |
| `intz_weight` | `1.0` | weight of `intz` in the score |
| `trend_strength_min` | `0.0` | min `|price/EMA-1|` for the trend filter |
| `autocorr_window` | `0` | rolling window for lag-1 return autocorrelation; 0 = off |
| `autocorr_mode` | `follow` | `follow` (only trade positive autocorr) or `fade` (flip in negative autocorr) |
| `autocorr_threshold` | `0.0` | min absolute autocorrelation |
| `vol_window` | `0` | rolling window for the volatility median; 0 = off |
| `vol_ratio_min` | `0.0` | min `sigPx / rolling_median(sigPx)` |
| `vol_ratio_max` | `1e9` | max `sigPx / rolling_median(sigPx)` |

The score is `pz_weight * pz - intz_weight * intz`. Setting
`intz_weight: 0` gives pure price momentum, which is a useful control and is
often better for 4–6h horizons than the default `comb`.

## Backtest

- One position at a time; signals while in position are ignored.
- Entry: close of the second in which the signal is produced
  (`close_last_ts` / `close_last`).
- Exit: first of TP, SL, timeout.
- TP/SL are checked on every 1s `high`/`low` of the holding period.
- Same 1s bar hits TP and SL -> SL wins when
  `conservative_intrabar: true`.
- Fees: `fee_bps` on **both** entry and exit (default 16 bps per side).
- Timeout closes at the last available 1s close before `max_hold_sec`.
- Short PnL is `-(exit/entry - 1)` (fixed; the old code used `entry/exit-1`).
- Trades are clipped to the fold boundary; positions still open at the test
  end are closed at the last available close.

Note: entry at the close of the signal second is what the specification asks
for, but it is slightly optimistic for live execution (the signal is only
known after that second closes). Add a realistic entry delay before trusting
absolute PnL numbers.

## Walk-forward

- Day-based splits. `train_days` / `test_days` can be set explicitly; when
  omitted, `block = total_days // (n_folds + 1)` and both are set to `block`.
- `walk_forward_mode`: `expanding` (train from the first day) or `rolling`
  (fixed-length train window).
- Indicator history is computed **causally on the full series** and then sliced.
  This gives the test fold a proper warm-up without leaking future data.
- Objective: `sharpe` (`sharpe_trades`), `profit_factor`, `net_pnl`
  (`net_pnl_pct`) or `calmar`. Aliases are normalised.
- `min_trades` / `min_win_rate` reject unreliable parameter sets.
- Risk constraints: `min_profit_factor`, `max_drawdown_pct`,
  `min_expectancy_pct`, and `min_total_trades` (used for the final top-10
  table). A train config that violates a constraint is never selected.
- Metrics are trade-based Sharpe `mean/std(ddof=1) * sqrt(N)` (not annualised),
  net PnL%, win rate (net of fees), gross win rate, profit factor, max drawdown
  on the closed-trade equity curve, avg hold, Buy/Sell signal counts,
  % time in position, Calmar, expectancy, and TP/SL/timeout breakdown.

## Outputs (`output/`)

- `results_per_fold.csv` — best train config per fold + train/test metrics.
- `aggregate_test.csv` — median/mean/std/min/max and % profitable folds.
- `all_configs_test_metrics.csv` — test metrics of **every** config on every fold.
- `all_configs_test_summary.csv` — per-config medians over folds.
- `top10_configs.csv` — top-10 by **median(test_sharpe)** across all test folds.
- `top10_expectancy.csv` — top-10 by **median(test_expectancy_pct)** with
  `min_total_trades`, `min_profit_factor` and `max_drawdown_pct` filters.
- `summary.json` — run metadata and elapsed time.

## Performance

The original implementation recomputed the indicator in Python loops and
rescanned every second for every interval (`O(n_intervals * max_hold)`).
The current version:

- vectorises interval aggregation and rolling statistics;
- computes raw PFD components once per `(interval, W, price_source)`;
- reuses signal arrays across all TP/SL/hold combinations;
- backtests with NumPy first-passage searches instead of pandas row iteration.

On this machine the full `config.yaml` grid (18,144 configs × 7 folds) over the
9.7-day file runs in ~4 minutes instead of ~2 hours. The reduced
`config_small.yaml` runs in ~1.5 seconds.

## Expectancy + risk-constrained search (6-24h)

`config_expectancy.yaml` runs a 6-24h grid with:

```yaml
objective: expectancy
min_trades: 3
min_profit_factor: 1.2
max_drawdown_pct: 2.0
min_expectancy_pct: 0.0
min_total_trades: 20
```

Result (10,368 configs, 51,840 backtests, ~95 s): best mean test expectancy
configuration is `interval=60s, W=120, sigma=3.0, confirm=1, intz_weight=1.0,
vol_window=720, vol_ratio>=0.8, TP=2%, SL=0.5%, hold=24h`:

- 22 test trades
- mean expectancy +0.432%/trade, median +0.266%
- mean net PnL +1.684%/fold, median +1.332%
- mean win rate 55.3%, median 60%
- median PF 1.81, median max DD 0.91%
- 80% profitable folds

It is saved in `config_expectancy_best.yaml`. This is an ex-post evaluation
on 36 days; fold 4 is negative and the sample is small, so out-of-sample
validation is required.

## Regime-filter test results (original comb, intz_weight = 1.0)

A separate 5-fold rolling run on the 35-day file tested 62,208 configurations
with trend-strength, autocorrelation and volatility filters (311,040
backtests, ~9 min). The baseline without regime filters had **zero**
configurations with >=10 test trades and positive mean net PnL.

The volatility filter was the only one that helped systematically:

- `vol_window=720` (12h at 60s), `vol_ratio=0.8..1.2` selects normal-volatility
  periods.
- Best balanced config: `interval=60s, W=120, sigma=3.0, direction=both,
  TP=1%, SL=0.5%, hold=4h` -> 23 test trades, mean win rate 60.8%,
  mean net PnL +0.321%/fold, 80% profitable folds, median Sharpe 0.14.
  It is saved in `config_regime.yaml`.
- Best by median test Sharpe (`vol_ratio_min=0.8`, no upper cap,
  TP=1%, SL=0.75%): 29 trades, median Sharpe 0.23, mean net PnL +0.10%,
  60% profitable folds.
- Autocorrelation (`follow` or `fade`) and pure trend-strength filters did not
  produce a positive configuration with >=20 trades in this sample.

This is an improvement over the no-filter baseline, but it is still a modest
and regime-dependent edge, not a reliable 70% accuracy.

## Accuracy findings (35-day file, Sep–Oct 2026)

Directional accuracy was measured on forward returns (before TP/SL path) and
with the event-driven backtest.

- The default `comb = pz - intz` has only ~50–60% directional accuracy at
  1–4h horizons over the whole sample.
- `pz` alone (`intz_weight: 0`) is better for longer horizons: at
  `interval=60s, W=360`, 4h horizon, extreme `pz` + aligned EMA filter gives
  ~73–76% directional accuracy and positive return after 0.32% round-trip
  costs — **but only in trending regimes**.
- Those signals cluster in the two strong trend blocks of the sample. In the
  three middle weeks (range/reversal) the same rule gives 0–30% accuracy.
  Therefore 70% is not robust on this file; it is regime-dependent.
- In the walk-forward test, the best configs with at least 20 test trades have
  median test Sharpe ~0.2–0.6, median win rate ~50–60%, and are still fragile.
  Configs that show 70–78% win rate have only 7–11 test trades, which is not
  statistically meaningful.

Recommendations before trusting a 70% target:

1. Collect more history covering several trend/range/reversal regimes.
2. Prefer pure `pz` (`intz_weight = 0`) or a smaller `intz_weight` for 4–6h
   horizons; the `intz` term is mostly a short-horizon flow-exhaustion signal.
3. Use a regime filter (trend strength / autocorrelation / volatility) and
   report accuracy per regime, not only pooled.
4. Consider time exits at 4–6h with wider stops; 0.5–1% TP with 0.16% per-side
   cost needs either >70% accuracy (symmetric TP/SL) or a favourable TP/SL
   ratio.
5. Validate on out-of-sample data collected after the parameter search.

## Tests / sanity checks

There is no standalone test suite, but the following were checked manually:

- Python PFD components match a direct naive implementation of the C++ loops.
- The NumPy backtester matches a row-by-row reference implementation on
  synthetic data (TP/SL/timeout, conservative same-bar rule, long/short PnL).
- Short PnL now uses the correct sign.
