#!/usr/bin/env python3
"""Test volatility-adaptive TP/SL on the best 6-24h PFD configuration.

Fixed entry:
    interval=60s, W=120, sigma=3.0, confirm=1, intz_weight=1.0,
    vol_ratio >= 0.8, SL=0.5%, max_hold=24h.

The hypothesis: use TP 1.5% in calm regimes and TP 2.0% in volatile regimes
while keeping SL 0.5%. Two regime definitions are tested:
  - ATR ratio: ATR(window) / rolling_median(ATR, 720)
  - PFD vol_ratio: sigPx / rolling_median(sigPx, 720)
Also tested: inverse rule and continuous TP = clip(mult * ATR%, 1%, 3%).

Example:
    python research_adaptive_tp.py --data ../aggregated_1s/aggregated_1s.csv \
        --out output/research
"""
from __future__ import annotations

import argparse
from itertools import product
from pathlib import Path

import numpy as np
import pandas as pd

from pfd_optimizer.backtest import run_backtest
from pfd_optimizer.data_loader import df_to_arrays, load_aggregated_1s
from pfd_optimizer.metrics import compute_metrics, time_in_position_sec
from pfd_optimizer.pfd_indicator import (
    aggregate_intervals,
    compute_pfd_components,
    compute_regime_features,
    compute_signals_from_components,
)
from pfd_optimizer.walk_forward import _make_splits


def _atr_features(d: pd.DataFrame, atr_window: int, median_window: int = 720) -> tuple[np.ndarray, np.ndarray]:
    high = d["high_max"].astype(float)
    low = d["low_min"].astype(float)
    close = d["close_last"].astype(float)
    prev_close = close.shift(1)
    tr = pd.concat(
        [high - low, (high - prev_close).abs(), (low - prev_close).abs()],
        axis=1,
    ).max(axis=1)
    atr = tr.rolling(atr_window, min_periods=atr_window).mean()
    base = atr.rolling(median_window, min_periods=median_window).median()
    ratio = atr / base
    pct = atr / close
    return ratio.to_numpy(dtype=float), pct.to_numpy(dtype=float)


def _summarize(trades: pd.DataFrame, fold_metrics: list[dict], total_sec: float, in_pos: float) -> dict:
    pnl = trades["pnl_pct"].to_numpy(dtype=np.float64)
    wins = pnl[pnl > 0]
    losses = pnl[pnl < 0]
    reasons = trades["exit_reason"].value_counts().to_dict()
    return {
        "trades": int(len(trades)),
        "win_rate": float(np.mean(pnl > 0)),
        "mean_pnl_pct": float(np.mean(pnl)),
        "profit_factor": float(wins.sum() / -losses.sum()) if losses.sum() < 0 else 999.0,
        "mean_fold_net_pct": float(np.mean([m["net_pnl_pct"] for m in fold_metrics])),
        "median_fold_net_pct": float(np.median([m["net_pnl_pct"] for m in fold_metrics])),
        "pct_profitable_folds": float(np.mean([m["net_pnl_pct"] > 0 for m in fold_metrics]) * 100.0),
        "mean_expectancy_pct": float(np.mean([m["expectancy_pct"] for m in fold_metrics])),
        "mean_sharpe": float(np.mean([m["sharpe_trades"] for m in fold_metrics])),
        "median_sharpe": float(np.median([m["sharpe_trades"] for m in fold_metrics])),
        "median_max_dd_pct": float(np.median([m["max_drawdown_pct"] for m in fold_metrics])),
        "mean_max_dd_pct": float(np.mean([m["max_drawdown_pct"] for m in fold_metrics])),
        "time_in_pos_pct": float(in_pos / total_sec * 100.0) if total_sec else 0.0,
        "avg_hold_sec": float(trades["hold_sec"].mean()),
        "exit_tp": int(reasons.get("tp", 0)),
        "exit_sl": int(reasons.get("sl", 0)),
        "exit_timeout": int(reasons.get("timeout", 0)),
    }


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--data", default="../aggregated_1s/aggregated_1s.csv")
    p.add_argument("--interval", type=int, default=60)
    p.add_argument("--window", type=int, default=120)
    p.add_argument("--sigma", type=float, default=3.0)
    p.add_argument("--confirm", type=int, default=1)
    p.add_argument("--vol-window", type=int, default=720)
    p.add_argument("--vol-min", type=float, default=0.8)
    p.add_argument("--max-hold-sec", type=int, default=86400)
    p.add_argument("--fee-bps", type=float, default=16.0)
    p.add_argument("--folds", type=int, default=7)
    p.add_argument("--train-days", type=int, default=6)
    p.add_argument("--test-days", type=int, default=6)
    p.add_argument("--atr-windows", default="60,240,720")
    p.add_argument("--thresholds", default="0.8,0.9,1.0,1.1,1.2")
    p.add_argument("--atr-mults", default="1.5,2.0,3.0,4.0,5.0")
    p.add_argument("--out", default="output/research")
    args = p.parse_args()

    df = load_aggregated_1s(args.data)
    arrays_1s = df_to_arrays(df)
    d = aggregate_intervals(df, args.interval)
    comp = compute_pfd_components(d, args.window, "vwap")
    signal = compute_signals_from_components(
        d,
        comp,
        args.sigma,
        signal_mode="momentum",
        confirm_intervals=args.confirm,
        trend_ema_span=0,
        direction="both",
        pz_weight=1.0,
        intz_weight=1.0,
    )
    regime = compute_regime_features(
        d, comp, price_source="vwap", autocorr_window=0, vol_window=args.vol_window
    )
    vr = regime["vol_ratio"]
    signal = np.where(np.isfinite(vr) & (vr >= args.vol_min), signal, 0).astype(np.int8)

    ts = d["close_last_ts"].to_numpy(dtype=np.int64)
    px = d["close_last"].to_numpy(dtype=np.float64)
    splits = _make_splits(
        int(df["ts"].min()), int(df["ts"].max()),
        args.folds, "rolling", args.train_days, args.test_days,
    )

    atr_windows = [int(x) for x in args.atr_windows.split(",")]
    thresholds = [float(x) for x in args.thresholds.split(",")]
    atr_mults = [float(x) for x in args.atr_mults.split(",")]

    atr_cache = {w: _atr_features(d, w) for w in atr_windows}

    # Variant = (name, tp_array, sl_array)
    variants: list[tuple[str, np.ndarray | None, np.ndarray | None]] = [
        ("fixed_tp2_sl0.5", None, None),
        ("fixed_tp1.5_sl0.5", np.full(len(d), 0.015), np.full(len(d), 0.005)),
    ]

    # Discrete ATR regime: calm TP 1.5, volatile TP 2.0, SL 0.5.
    for atr_w, threshold in product(atr_windows, thresholds):
        ratio, _ = atr_cache[atr_w]
        calm = np.isfinite(ratio) & (ratio < threshold)
        tp = np.where(calm, 0.015, 0.020)
        sl = np.full(len(d), 0.005)
        variants.append((f"atr{atr_w}_lt{threshold:.2f}_tp1.5_2.0", tp, sl))
        # Inverse rule as a direction check.
        tp_inv = np.where(calm, 0.020, 0.015)
        variants.append((f"atr{atr_w}_lt{threshold:.2f}_tp2.0_1.5", tp_inv, sl))

    # PFD vol_ratio regime.
    for threshold in thresholds:
        calm = np.isfinite(vr) & (vr < threshold)
        tp = np.where(calm, 0.015, 0.020)
        sl = np.full(len(d), 0.005)
        variants.append((f"volratio_lt{threshold:.2f}_tp1.5_2.0", tp, sl))

    # Continuous ATR-scaled TP, capped to [1%, 3%], SL 0.5%.
    for atr_w, mult in product(atr_windows, atr_mults):
        _, pct = atr_cache[atr_w]
        tp = np.clip(mult * pct, 0.010, 0.030)
        tp = np.where(np.isfinite(tp), tp, 0.020)
        sl = np.full(len(d), 0.005)
        variants.append((f"atr{atr_w}_clip_{mult:.1f}xatr", tp, sl))

    rows = []
    for name, tp_arr, sl_arr in variants:
        all_trades = []
        fold_metrics = []
        for tr_s, tr_e, te_s, te_e in splits:
            mask = (ts >= te_s) & (ts <= te_e) & (signal != 0)
            idx = np.flatnonzero(mask)
            if len(idx) == 0:
                continue
            if tp_arr is None:
                res = run_backtest(
                    ts[idx], signal[idx], px[idx], arrays_1s,
                    0.020, 0.005, args.max_hold_sec,
                    args.fee_bps, True, te_s, te_e,
                )
            else:
                res = run_backtest(
                    ts[idx], signal[idx], px[idx], arrays_1s,
                    0.020, 0.005, args.max_hold_sec,
                    args.fee_bps, True, te_s, te_e,
                    take_profit_arr=tp_arr[idx],
                    stop_loss_arr=sl_arr[idx],
                )
            trades = res.to_frame()
            all_trades.append(trades)
            if len(trades):
                total_sec = (te_e - te_s) / 1000.0
                in_pos = time_in_position_sec(res.entry_ts, res.exit_ts, te_s, te_e)
                fold_metrics.append(
                    compute_metrics(
                        trades,
                        int(np.sum(signal[mask] == 1)),
                        int(np.sum(signal[mask] == -1)),
                        in_pos,
                        total_sec,
                        fee_bps=args.fee_bps,
                    )
                )

        trades = pd.concat(all_trades, ignore_index=True) if all_trades else pd.DataFrame()
        if trades.empty:
            continue
        total_sec = sum((te_e - te_s) / 1000.0 for _, _, te_s, te_e in splits)
        in_pos = 0.0
        for tr_s, tr_e, te_s, te_e in splits:
            part = trades[(trades["entry_ts"] >= te_s) & (trades["entry_ts"] <= te_e)]
            if len(part):
                in_pos += time_in_position_sec(
                    part["entry_ts"].to_numpy(), part["exit_ts"].to_numpy(), te_s, te_e
                )
        row = {"variant": name}
        row.update(_summarize(trades, fold_metrics, total_sec, in_pos))
        rows.append(row)

    out = pd.DataFrame(rows)
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    path = outdir / "adaptive_tp_summary.csv"
    out.to_csv(path, index=False)

    pd.set_option("display.width", 380)
    pd.set_option("display.max_columns", 60)
    cols = [
        "variant", "trades", "win_rate", "mean_pnl_pct", "profit_factor",
        "mean_fold_net_pct", "median_fold_net_pct", "pct_profitable_folds",
        "mean_expectancy_pct", "mean_sharpe", "median_sharpe",
        "median_max_dd_pct", "time_in_pos_pct", "avg_hold_sec",
        "exit_tp", "exit_sl", "exit_timeout",
    ]
    print(out[cols].sort_values(
        ["mean_fold_net_pct", "mean_pnl_pct"], ascending=False
    ).to_string(index=False))
    print(f"Saved {path}")


if __name__ == "__main__":
    main()
