# PFD Hyperparameter Optimizer

Offline grid search + walk-forward for **Price-Flow Divergence (PFD)** indicator on Bybit 1s aggregated data.

## Input

CSV: `research_on_data/aggregated_1s/aggregated_1s.csv`

Columns: `ts, open, high, low, close, buy_vol, sell_vol, trade_count, vwap`
- `ts` — UTC ms (start of second), int64
- prices float, volumes float, `trade_count` int, `vwap` float

## Install

```bash
cd research_on_data/pfd_optimizer
pip install -r requirements.txt
```

## Run

```bash
python run_optimization.py --config config.yaml --data ../aggregated_1s/aggregated_1s.csv --output output
```

Or:
```bash
bash example_run.sh
```

## Config (config.yaml)

Grid matches spec:
- `interval_sec`: [10,20,30,60,120,180,300]
- `window_size W`: [30,60,120,180,360,720]
- `sigma`: [1.0,1.5,2.0,2.5,3.0,3.5]
- `price_source`: [vwap, close]
- `take_profit`: [0.005,0.0075,0.01] (0.5–1.0%)
- `stop_loss`: [0.0025,0.005,0.01]
- `max_hold_sec`: [1800,3600,7200,14400]

Walk-forward:
- `n_folds` (default 5), `walk_forward_mode` {expanding, rolling}, `objective` {sharpe, profit_factor, net_pnl, calmar}

Backtest:
- `fee_bps` 16.0 (0.16%), `conservative_intrabar` true (SL first on same-second TP/SL)

## PFD logic (matches C++)

1. Aggregate by `interval_sec`: `buyAcc`, `sellAcc`, `flow=(buy-sell)/(buy+sell)` if sum>1e-12 else 0. `price_mid` = last second `vwap` or `close` by `price_source`.
2. Buffers `price_series`, `flow_series` length `W+1`. Signal only when filled.
3. `tr = price_series[-1]/price_series[0]-1` (W intervals)
4. `rets_i = price_series[i]/price_series[i-1]-1`, i in 1..W, `sigPx = sqrt(var(rets, ddof=1))*sqrt(W)`
5. `flow_window = flow_series[-W:]`, `stdFlow = sqrt(var(flow_window,ddof=1))`, `intz = meanFlow*sqrt(W)/stdFlow`
6. Guards `sigPx<1e-9 or stdFlow<1e-9` -> `comb=0`. `comb = tr/sigPx - intz`. Signal +1 if `comb>=sigma`, -1 if `comb<=-sigma`, else 0.

## Backtest

- One position. New signals ignored while in position.
- Entry: on signal, entry price = `close` of the signal second/interval (close_interval/close_last).
- Exit first of: TP, SL (by high/low intrabar), timeout `max_hold_sec`. If TP and SL in same second -> SL first (conservative). Fees 2x (entry+exit).

## Metrics

Net PnL%, Trades, Win rate, Profit Factor, **Sharpe by trades** `mean/std*sqrt(N)` (per-trade, not annualized), Max DD (closed-trade equity), Avg hold sec, Buy/Sell signals, % time in position, Calmar, Expectancy, Avg win/loss, Largest win/loss.

## Outputs (output/)

- `results_per_fold.csv` — train/test metrics + best params per fold
- `aggregate_test.csv` — median/mean/std/min/max + % profitable folds
- `top10_configs.csv` — top-10 by `median(test_sharpe)` (based on fold bests)
- `summary.json`

## Notes

Grid can be large (thousands). For quick tests reduce ranges in `config.yaml`. Walk-forward uses calendar day splits. All calculations match C++ logic precisely.
