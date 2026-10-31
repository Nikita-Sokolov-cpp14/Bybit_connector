#!/usr/bin/env python3
"""Stability scan for volatility-adaptive TP thresholds.

Rule:
    vol_ratio < T -> TP 1.5%, else TP 2.0%; SL = 0.5%, max_hold = 24h.

The goal is not to find the best historical threshold, but a threshold that is
stable across folds:
    - enough trades in both calm and volatile buckets;
    - good median/net PnL and win rate;
    - low dispersion and decent worst fold;
    - similar performance on the first (older) and last (newer) folds.

Example:
    python research_vol_thresholds.py --data ../aggregated_1s/aggregated_1s.csv \
        --out output/research
"""
from __future__ import annotations

import argparse
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
    p.add_argument(
        "--thresholds",
        default="0.80,0.85,0.90,0.95,1.00,1.05,1.10,1.15,1.20,1.25,1.30,1.40,1.50",
    )
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

    # Distribution of vol_ratio among entry signals (useful for interpreting T).
    sig_mask = signal != 0
    vr_sig = vr[sig_mask]
    vr_sig = vr_sig[np.isfinite(vr_sig)]
    print("vol_ratio among signals:",
          {q: round(float(np.quantile(vr_sig, q)), 3) for q in [0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0]},
          "n=", len(vr_sig))

    thresholds = [float(x) for x in args.thresholds.split(",")]
    fold_rows: list[dict] = []

    def run_variant(name: str, threshold: float | None, fixed_tp: float | None = None) -> None:
        for fold, (tr_s, tr_e, te_s, te_e) in enumerate(splits, start=1):
            mask = (ts >= te_s) & (ts <= te_e) & (signal != 0)
            idx = np.flatnonzero(mask)
            if len(idx) == 0:
                continue
            if threshold is None:
                res = run_backtest(
                    ts[idx], signal[idx], px[idx], arrays_1s,
                    float(fixed_tp), 0.005, args.max_hold_sec,
                    args.fee_bps, True, te_s, te_e,
                )
            else:
                calm = np.isfinite(vr[idx]) & (vr[idx] < threshold)
                tp = np.where(calm, 0.015, 0.020)
                sl = np.full(len(idx), 0.005)
                res = run_backtest(
                    ts[idx], signal[idx], px[idx], arrays_1s,
                    0.020, 0.005, args.max_hold_sec,
                    args.fee_bps, True, te_s, te_e,
                    take_profit_arr=tp,
                    stop_loss_arr=sl,
                )
            trades = res.to_frame()
            if trades.empty:
                continue
            # Count calm/volatile among actual trades (entry_ts -> interval index).
            entry_pos = np.searchsorted(ts, trades["entry_ts"].to_numpy(dtype=np.int64), side="left")
            valid_pos = (entry_pos >= 0) & (entry_pos < len(ts))
            if threshold is None:
                calm_n = 0
            else:
                calms = np.zeros(len(trades), dtype=bool)
                calms[valid_pos] = (
                    np.isfinite(vr[entry_pos[valid_pos]]) & (vr[entry_pos[valid_pos]] < threshold)
                )
                calm_n = int(np.sum(calms))
            total_sec = (te_e - te_s) / 1000.0
            in_pos = time_in_position_sec(res.entry_ts, res.exit_ts, te_s, te_e)
            m = compute_metrics(
                trades,
                int(np.sum(signal[mask] == 1)),
                int(np.sum(signal[mask] == -1)),
                in_pos,
                total_sec,
                fee_bps=args.fee_bps,
            )
            fold_rows.append(
                {
                    "variant": name,
                    "threshold": threshold,
                    "fold": fold,
                    "trades": len(trades),
                    "calm_trades": calm_n,
                    "volatile_trades": int(len(trades) - calm_n) if threshold is not None else int(len(trades)),
                    "win_rate": m["win_rate"],
                    "net_pnl_pct": m["net_pnl_pct"],
                    "expectancy_pct": m["expectancy_pct"],
                    "profit_factor": m["profit_factor"],
                    "sharpe_trades": m["sharpe_trades"],
                    "max_drawdown_pct": m["max_drawdown_pct"],
                    "tp_count": m["tp_count"],
                    "sl_count": m["sl_count"],
                    "timeout_count": m["timeout_count"],
                }
            )

    run_variant("fixed_tp2.0", None, fixed_tp=0.020)
    run_variant("fixed_tp1.5", None, fixed_tp=0.015)
    for threshold in thresholds:
        run_variant(f"vol_lt_{threshold:.2f}", threshold)

    folds_df = pd.DataFrame(fold_rows)
    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    folds_path = outdir / "vol_threshold_fold_metrics.csv"
    folds_df.to_csv(folds_path, index=False)

    # Aggregate by variant.
    rows = []
    for variant, g in folds_df.groupby("variant", sort=False):
        pnl = g["net_pnl_pct"].to_numpy(dtype=float)
        first5 = g[g["fold"] <= 5]["net_pnl_pct"].to_numpy(dtype=float)
        last2 = g[g["fold"] >= 6]["net_pnl_pct"].to_numpy(dtype=float)
        row = {
            "variant": variant,
            "threshold": g["threshold"].iloc[0],
            "folds": len(g),
            "trades": int(g["trades"].sum()),
            "calm_trades": int(g["calm_trades"].sum()),
            "volatile_trades": int(g["volatile_trades"].sum()),
            "mean_fold_net": float(np.mean(pnl)),
            "median_fold_net": float(np.median(pnl)),
            "std_fold_net": float(np.std(pnl, ddof=1)) if len(pnl) > 1 else 0.0,
            "min_fold_net": float(np.min(pnl)),
            "max_fold_net": float(np.max(pnl)),
            "mean_minus_half_std": float(np.mean(pnl) - 0.5 * np.std(pnl, ddof=1)) if len(pnl) > 1 else float(np.mean(pnl)),
            "pct_profitable_folds": float(np.mean(pnl > 0) * 100.0),
            "mean_win_rate": float(np.average(g["win_rate"], weights=g["trades"])),
            "median_win_rate": float(g["win_rate"].median()),
            "median_sharpe": float(g["sharpe_trades"].median()),
            "min_sharpe": float(g["sharpe_trades"].min()),
            "median_max_dd": float(g["max_drawdown_pct"].median()),
            "mean_max_dd": float(g["max_drawdown_pct"].mean()),
            "first5_mean_net": float(np.mean(first5)) if len(first5) else np.nan,
            "last2_mean_net": float(np.mean(last2)) if len(last2) else np.nan,
            "last2_trades": int(g[g["fold"] >= 6]["trades"].sum()),
            "worst_fold_win": float(g["win_rate"].min()),
        }
        rows.append(row)
    summary = pd.DataFrame(rows)
    summary_path = outdir / "vol_threshold_summary.csv"
    summary.to_csv(summary_path, index=False)

    pd.set_option("display.width", 400)
    pd.set_option("display.max_columns", 80)
    cols = [
        "variant", "trades", "calm_trades", "volatile_trades",
        "mean_fold_net", "median_fold_net", "std_fold_net", "min_fold_net",
        "mean_minus_half_std", "pct_profitable_folds",
        "mean_win_rate", "median_win_rate", "median_sharpe", "min_sharpe",
        "median_max_dd", "first5_mean_net", "last2_mean_net", "last2_trades",
    ]
    print("\nSorted by median fold net:")
    print(summary.sort_values("median_fold_net", ascending=False)[cols].to_string(index=False))
    print("\nSorted by stability score (mean - 0.5*std):")
    print(summary.sort_values("mean_minus_half_std", ascending=False)[cols].to_string(index=False))
    print(f"\nSaved {folds_path}")
    print(f"Saved {summary_path}")


if __name__ == "__main__":
    main()
