#!/usr/bin/env python3
"""Directional-accuracy research for the PFD score.

This does not run the full backtest. It answers a simpler question: after a
signal, what is the probability that the price is higher (long) / lower (short)
after a fixed horizon, and what is the mean return after fees?

Example:
    python research_pfd.py --data ../aggregated_1s/aggregated_1s.csv \
        --interval 60 --window 360 --horizons 120,240,360 \
        --intz-weights 0,0.5,1.0 --out output/research
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd

from pfd_optimizer.data_loader import load_aggregated_1s
from pfd_optimizer.pfd_indicator import aggregate_intervals, compute_pfd_components


def _ema(values: np.ndarray, span: int) -> np.ndarray:
    return pd.Series(values).ewm(span=span, adjust=False).mean().to_numpy()


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--data", default="../aggregated_1s/aggregated_1s.csv")
    p.add_argument("--interval", type=int, default=60)
    p.add_argument("--window", type=int, default=360)
    p.add_argument("--price-source", choices=["vwap", "close"], default="vwap")
    p.add_argument("--horizons", default="120,240,360", help="minutes, comma-separated")
    p.add_argument("--quantiles", default="0.05,0.025,0.01", help="two-sided tail mass")
    p.add_argument("--intz-weights", default="0.0,0.5,1.0", help="comma-separated weights")
    p.add_argument("--trend-emas", default="0,120,360", help="EMA spans in intervals (0=off)")
    p.add_argument("--fee-bps", type=float, default=16.0)
    p.add_argument("--blocks", type=int, default=5, help="chronological blocks for stability")
    p.add_argument("--out", default="output/research")
    args = p.parse_args()

    data = load_aggregated_1s(args.data)
    d = aggregate_intervals(data, args.interval)
    comp = compute_pfd_components(d, args.window, args.price_source)
    prices = (
        d["vwap_last"].to_numpy(dtype=np.float64)
        if args.price_source == "vwap"
        else d["close_last"].to_numpy(dtype=np.float64)
    )
    valid = comp["valid"]
    ts = d["ts_interval"].to_numpy()

    horizons = [int(x) for x in args.horizons.split(",")]
    quantiles = [float(x) for x in args.quantiles.split(",")]
    weights = [float(x) for x in args.intz_weights.split(",")]
    ema_spans = [int(x) for x in args.trend_emas.split(",")]
    fee = args.fee_bps / 10000.0
    block_size = max(1, len(d) // max(1, args.blocks))
    block_id = np.arange(len(d)) // block_size

    rows = []
    for w in weights:
        score = comp["pz"] - w * comp["intz"]
        for span in ema_spans:
            if span > 1:
                ema = _ema(prices, span)
                long_ok = prices > ema
                short_ok = prices < ema
            else:
                long_ok = np.ones(len(d), dtype=bool)
                short_ok = np.ones(len(d), dtype=bool)

            for minutes in horizons:
                h = max(1, minutes * 60 // args.interval)
                fwd = np.full(len(prices), np.nan)
                fwd[:-h] = prices[h:] / prices[:-h] - 1.0
                base = valid & np.isfinite(fwd) & np.isfinite(score)

                for q in quantiles:
                    x = score[base]
                    if len(x) < 100:
                        continue
                    lo, hi = np.quantile(x, [q, 1.0 - q])
                    long_sel = base & (score >= hi) & long_ok
                    short_sel = base & (score <= lo) & short_ok

                    long_ret = fwd[long_sel]
                    short_ret = -fwd[short_sel]
                    n_long = int(len(long_ret))
                    n_short = int(len(short_ret))
                    if n_long + n_short == 0:
                        continue

                    acc_long = float(np.mean(long_ret > 0)) if n_long else np.nan
                    acc_short = float(np.mean(short_ret > 0)) if n_short else np.nan
                    n_total = n_long + n_short
                    acc = (
                        (np.sum(long_ret > 0) + np.sum(short_ret > 0)) / n_total
                        if n_total
                        else np.nan
                    )
                    net_long = float(np.mean(long_ret) - 2 * fee) if n_long else np.nan
                    net_short = float(np.mean(short_ret) - 2 * fee) if n_short else np.nan

                    # Accuracy per chronological block (stability check).
                    block_acc = []
                    for b in range(args.blocks):
                        bl = long_sel & (block_id == b)
                        bs = short_sel & (block_id == b)
                        nb = int(bl.sum() + bs.sum())
                        if nb >= 10:
                            block_acc.append(
                                float((np.sum(fwd[bl] > 0) + np.sum(-fwd[bs] > 0)) / nb)
                            )

                    rows.append(
                        {
                            "interval_sec": args.interval,
                            "window_size": args.window,
                            "price_source": args.price_source,
                            "intz_weight": w,
                            "trend_ema_span": span,
                            "horizon_min": minutes,
                            "tail_q": q,
                            "n_long": n_long,
                            "n_short": n_short,
                            "acc_long": acc_long,
                            "acc_short": acc_short,
                            "acc_all": float(acc),
                            "net_long_pct": net_long * 100.0 if n_long else np.nan,
                            "net_short_pct": net_short * 100.0 if n_short else np.nan,
                            "block_min_acc": float(min(block_acc)) if block_acc else np.nan,
                            "block_max_acc": float(max(block_acc)) if block_acc else np.nan,
                            "block_accs": " ".join(f"{v:.2f}" for v in block_acc),
                        }
                    )

    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    df = pd.DataFrame(rows)
    path = outdir / "direction_accuracy.csv"
    df.to_csv(path, index=False)

    if not df.empty:
        cols = [
            "intz_weight",
            "trend_ema_span",
            "horizon_min",
            "tail_q",
            "n_long",
            "n_short",
            "acc_long",
            "acc_short",
            "acc_all",
            "net_long_pct",
            "net_short_pct",
            "block_accs",
        ]
        print(df.sort_values("acc_all", ascending=False)[cols].head(20).to_string(index=False))
    print(f"Saved {path}")


if __name__ == "__main__":
    main()
