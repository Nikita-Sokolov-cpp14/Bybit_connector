#!/usr/bin/env python3
"""Test trailing-stop variants on the best 6-24h PFD configuration.

Baseline entry:
    interval=60s, W=120, sigma=3.0, confirm=1, intz_weight=1.0,
    vol_ratio >= 0.8, TP=2%, initial SL=0.5%, max_hold=24h.

The trailing stop starts after ``activation`` profit and trails the best
high/low by ``distance``. The initial SL 0.5% remains in place before that.

Example:
    python research_trailing.py --data ../aggregated_1s/aggregated_1s.csv \
        --out output/research
"""
from __future__ import annotations

import argparse
from itertools import product
from pathlib import Path

import numpy as np
import pandas as pd

from pfd_optimizer.backtest import run_backtest, run_backtest_trailing
from pfd_optimizer.data_loader import df_to_arrays, load_aggregated_1s
from pfd_optimizer.metrics import compute_metrics, time_in_position_sec
from pfd_optimizer.pfd_indicator import (
    aggregate_intervals,
    compute_pfd_components,
    compute_regime_features,
    compute_signals_from_components,
)
from pfd_optimizer.walk_forward import _make_splits


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
        "exit_trail": int(reasons.get("trail", 0)),
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
    p.add_argument("--take-profit", type=float, default=0.02)
    p.add_argument("--stop-loss", type=float, default=0.005)
    p.add_argument("--max-hold-sec", type=int, default=86400)
    p.add_argument("--fee-bps", type=float, default=16.0)
    p.add_argument("--activations", default="0,0.0025,0.005,0.01,0.015,0.02")
    p.add_argument("--distances", default="0.0025,0.005,0.0075,0.01,0.015")
    p.add_argument("--folds", type=int, default=5)
    p.add_argument("--train-days", type=int, default=6)
    p.add_argument("--test-days", type=int, default=6)
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
    if args.vol_window > 1:
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

    activations = [float(x) for x in args.activations.split(",")]
    distances = [float(x) for x in args.distances.split(",")]
    variants = [(None, None)] + list(product(activations, distances))

    rows = []
    for activation, distance in variants:
        all_trades = []
        fold_metrics = []
        for tr_s, tr_e, te_s, te_e in splits:
            mask = (ts >= te_s) & (ts <= te_e) & (signal != 0)
            idx = np.flatnonzero(mask)
            if len(idx) == 0:
                continue
            if distance is None:
                res = run_backtest(
                    ts[idx], signal[idx], px[idx], arrays_1s,
                    args.take_profit, args.stop_loss, args.max_hold_sec,
                    args.fee_bps, True, te_s, te_e,
                )
            else:
                res = run_backtest_trailing(
                    ts[idx], signal[idx], px[idx], arrays_1s,
                    args.take_profit, args.stop_loss, args.max_hold_sec,
                    distance, activation, args.fee_bps, True, te_s, te_e,
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
        row = {"trail_activation": activation, "trail_distance": distance}
        row.update(_summarize(trades, fold_metrics, total_sec, in_pos))
        rows.append(row)

    out = pd.DataFrame(rows)
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    path = outdir / "trailing_test_summary.csv"
    out.to_csv(path, index=False)

    pd.set_option("display.width", 360)
    pd.set_option("display.max_columns", 50)
    cols = [
        "trail_activation", "trail_distance", "trades", "win_rate", "mean_pnl_pct",
        "profit_factor", "mean_fold_net_pct", "median_fold_net_pct",
        "pct_profitable_folds", "mean_expectancy_pct", "mean_sharpe",
        "median_sharpe", "median_max_dd_pct", "time_in_pos_pct", "avg_hold_sec",
        "exit_tp", "exit_sl", "exit_trail", "exit_timeout",
    ]
    print(out[cols].sort_values(
        ["mean_fold_net_pct", "profit_factor"], ascending=False
    ).to_string(index=False))
    print(f"Saved {path}")


if __name__ == "__main__":
    main()
