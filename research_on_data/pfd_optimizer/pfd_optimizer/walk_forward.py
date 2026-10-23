# Walk-forward optimization (vectorised indicator, event-driven backtest).
from __future__ import annotations

import json
import time
from itertools import product
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np
import pandas as pd
from tqdm import tqdm

from .backtest import run_backtest
from .data_loader import df_to_arrays, load_aggregated_1s
from .metrics import compute_metrics, score_metrics, time_in_position_sec
from .pfd_indicator import (
    aggregate_intervals,
    compute_pfd_components,
    compute_regime_features,
    compute_signals_from_components,
)

SIGNAL_PARAM_DEFAULTS = {
    "signal_mode": "momentum",
    "confirm_intervals": 0,
    "trend_ema_span": 0,
    "trend_filter_mode": "aligned",
    "direction": "both",
    "pz_weight": 1.0,
    "intz_weight": 1.0,
    "trend_strength_min": 0.0,
    "autocorr_window": 0,
    "autocorr_mode": "follow",
    "autocorr_threshold": 0.0,
    "vol_window": 0,
    "vol_ratio_min": 0.0,
    "vol_ratio_max": 1e9,
}
EXIT_PARAMS = ["take_profit", "stop_loss", "max_hold_sec"]
SIGNAL_PARAMS = ["interval_sec", "window_size", "sigma", "price_source"] + list(
    SIGNAL_PARAM_DEFAULTS
)
BASE_COMPONENT_PARAMS = ["interval_sec", "window_size", "price_source"]


def _as_list(value: Any) -> List[Any]:
    if isinstance(value, (list, tuple)):
        return list(value)
    return [value]


def _build_param_grid(cfg: Dict[str, Any]) -> Tuple[List[Dict[str, Any]], List[str]]:
    """Cartesian product of every parameter present in ``cfg['params']``."""
    ps = cfg["params"]
    keys = list(ps.keys())
    values_lists = [_as_list(ps[k]) for k in keys]
    grid = [dict(zip(keys, combo)) for combo in product(*values_lists)]
    for p in grid:
        for k, v in SIGNAL_PARAM_DEFAULTS.items():
            p.setdefault(k, v)
    return grid, keys


def _make_splits(
    ts_min: int,
    ts_max: int,
    n_folds: int,
    mode: str,
    train_days: int | None,
    test_days: int | None,
) -> List[Tuple[int, int, int, int]]:
    """
    Day-based walk-forward splits.

    Fold i has a test block of ``test_days`` days. Expanding: train starts at
    the first data day; rolling: train is the previous ``train_days`` days.
    """
    day_ms = 24 * 3600 * 1000
    start_day = int(ts_min // day_ms)
    end_day = int(ts_max // day_ms)
    total_days = max(1, end_day - start_day + 1)

    if train_days is None or test_days is None:
        block = max(1, total_days // (max(1, n_folds) + 1))
        if test_days is None:
            test_days = block
        if train_days is None:
            train_days = block

    test_days = max(1, int(test_days))
    train_days = max(1, int(train_days))

    splits: List[Tuple[int, int, int, int]] = []
    for i in range(int(n_folds)):
        test_start_day = train_days + i * test_days
        if test_start_day >= total_days:
            break
        test_end_day = min(total_days, test_start_day + test_days)
        if test_end_day <= test_start_day:
            break
        if mode == "rolling":
            train_start_day = max(0, test_start_day - train_days)
        elif mode == "expanding":
            train_start_day = 0
        else:
            raise ValueError("walk_forward_mode must be 'expanding' or 'rolling'")

        train_start = (start_day + train_start_day) * day_ms
        train_end = (start_day + test_start_day) * day_ms - 1
        test_start = (start_day + test_start_day) * day_ms
        test_end = (start_day + test_end_day) * day_ms - 1
        splits.append((int(train_start), int(train_end), int(test_start), int(test_end)))
    return splits


def _precompute_fold_signals(
    dint: pd.DataFrame,
    signal: np.ndarray,
    splits: List[Tuple[int, int, int, int]],
) -> List[dict]:
    """Slice one signal array into train/test arrays for every fold."""
    ts = dint["close_last_ts"].to_numpy(dtype=np.int64, copy=False)
    close_px = dint["close_last"].to_numpy(dtype=np.float64, copy=False)
    folds = []
    for tr_s, tr_e, te_s, te_e in splits:
        tr_mask = (ts >= tr_s) & (ts <= tr_e) & (signal != 0)
        te_mask = (ts >= te_s) & (ts <= te_e) & (signal != 0)
        tr_idx = np.flatnonzero(tr_mask)
        te_idx = np.flatnonzero(te_mask)
        folds.append(
            {
                "train": {
                    "ts": ts[tr_idx],
                    "dir": signal[tr_idx].astype(np.int8),
                    "px": close_px[tr_idx],
                    "start": tr_s,
                    "end": tr_e,
                    "all_buy": int(np.sum((ts >= tr_s) & (ts <= tr_e) & (signal == 1))),
                    "all_sell": int(np.sum((ts >= tr_s) & (ts <= tr_e) & (signal == -1))),
                },
                "test": {
                    "ts": ts[te_idx],
                    "dir": signal[te_idx].astype(np.int8),
                    "px": close_px[te_idx],
                    "start": te_s,
                    "end": te_e,
                    "all_buy": int(np.sum((ts >= te_s) & (ts <= te_e) & (signal == 1))),
                    "all_sell": int(np.sum((ts >= te_s) & (ts <= te_e) & (signal == -1))),
                },
            }
        )
    return folds


def _metrics_for(
    part: dict,
    arrays_1s: dict[str, np.ndarray],
    exit_cfg: dict,
    fee_bps: float,
    conservative: bool,
) -> dict:
    res = run_backtest(
        part["ts"],
        part["dir"],
        part["px"],
        arrays_1s,
        float(exit_cfg["take_profit"]),
        float(exit_cfg["stop_loss"]),
        int(exit_cfg["max_hold_sec"]),
        fee_bps,
        conservative,
        start_ts=int(part["start"]),
        end_ts=int(part["end"]),
    )
    total_sec = max(0.0, (int(part["end"]) - int(part["start"])) / 1000.0)
    in_pos = time_in_position_sec(res.entry_ts, res.exit_ts, part["start"], part["end"])
    trades_df = res.to_frame()
    return compute_metrics(
        trades_df,
        int(part["all_buy"]),
        int(part["all_sell"]),
        in_pos,
        total_sec,
        fee_bps=fee_bps,
    )


def _unique_dicts(dicts: List[dict], keys: List[str]) -> List[dict]:
    seen = set()
    out = []
    for d in dicts:
        key = tuple(d[k] for k in keys)
        if key in seen:
            continue
        seen.add(key)
        out.append({k: d[k] for k in keys})
    return out


def optimize(config_path: str, data_path: str, output_dir: str = "output") -> Dict[str, Any]:
    import yaml

    t_start = time.time()
    cfg = yaml.safe_load(Path(config_path).read_text())
    outdir = Path(output_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    wf_cfg = cfg.get("walk_forward", {})
    n_folds = int(wf_cfg.get("n_folds", 5))
    wf_mode = str(wf_cfg.get("walk_forward_mode", "expanding"))
    objective = str(wf_cfg.get("objective", "sharpe"))
    train_days = wf_cfg.get("train_days")
    test_days = wf_cfg.get("test_days")
    min_trades = int(wf_cfg.get("min_trades", 1))
    min_win_rate = float(wf_cfg.get("min_win_rate", 0.0))

    bt_cfg = cfg.get("backtest", {})
    fee_bps = float(bt_cfg.get("fee_bps", 16.0))
    conservative = bool(bt_cfg.get("conservative_intrabar", True))

    grid, param_keys = _build_param_grid(cfg)
    if not grid:
        raise ValueError("Empty parameter grid")

    df1 = load_aggregated_1s(data_path)
    arrays_1s = df_to_arrays(df1)
    splits = _make_splits(
        int(df1["ts"].min()),
        int(df1["ts"].max()),
        n_folds,
        wf_mode,
        int(train_days) if train_days is not None else None,
        int(test_days) if test_days is not None else None,
    )
    if not splits:
        raise ValueError("Not enough data for the requested number of folds")

    # Preserve config order for the output.
    signal_keys = _unique_dicts(grid, SIGNAL_PARAMS)
    exit_keys = _unique_dicts(grid, EXIT_PARAMS)

    # Group signal keys by parameters that affect the raw PFD components.
    groups: Dict[tuple, List[dict]] = {}
    for sk in signal_keys:
        base = tuple(sk[k] for k in BASE_COMPONENT_PARAMS)
        groups.setdefault(base, []).append(sk)

    n_evals = len(signal_keys) * len(exit_keys) * len(splits)
    best_by_fold: Dict[int, dict] = {}
    all_test_rows: List[dict] = []

    pbar = tqdm(total=n_evals, desc="walk-forward", unit="bt")
    done = 0

    for interval_sec in sorted({int(sk["interval_sec"]) for sk in signal_keys}):
        dint = aggregate_intervals(df1, interval_sec)
        for (_, window_size, price_source), group_keys in groups.items():
            if int(group_keys[0]["interval_sec"]) != interval_sec:
                continue
            comp = compute_pfd_components(dint, int(window_size), str(price_source))
            regime_cache: Dict[tuple, dict] = {}

            for sk in group_keys:
                trend_px = dint["vwap_last" if sk["price_source"] == "vwap" else "close_last"].to_numpy(
                    dtype=np.float64
                )
                regime_key = (int(sk["autocorr_window"]), int(sk["vol_window"]), str(sk["price_source"]))
                if regime_key not in regime_cache:
                    regime_cache[regime_key] = compute_regime_features(
                        dint,
                        comp,
                        price_source=str(sk["price_source"]),
                        autocorr_window=int(sk["autocorr_window"]),
                        vol_window=int(sk["vol_window"]),
                    )
                regime = regime_cache[regime_key]
                signal = compute_signals_from_components(
                    dint,
                    comp,
                    float(sk["sigma"]),
                    signal_mode=str(sk["signal_mode"]),
                    confirm_intervals=int(sk["confirm_intervals"]),
                    trend_ema_span=int(sk["trend_ema_span"]),
                    trend_filter_mode=str(sk["trend_filter_mode"]),
                    direction=str(sk["direction"]),
                    pz_weight=float(sk["pz_weight"]),
                    intz_weight=float(sk["intz_weight"]),
                    trend_prices=trend_px,
                    trend_strength_min=float(sk["trend_strength_min"]),
                    autocorr_window=int(sk["autocorr_window"]),
                    autocorr_mode=str(sk["autocorr_mode"]),
                    autocorr_threshold=float(sk["autocorr_threshold"]),
                    autocorr=regime.get("autocorr"),
                    vol_window=int(sk["vol_window"]),
                    vol_ratio_min=float(sk["vol_ratio_min"]),
                    vol_ratio_max=float(sk["vol_ratio_max"]),
                    vol_ratio=regime.get("vol_ratio"),
                )
                fold_parts = _precompute_fold_signals(dint, signal, splits)

                for exit_cfg in exit_keys:
                    for f, parts in enumerate(fold_parts, start=1):
                        key = tuple(sk[k] for k in SIGNAL_PARAMS) + tuple(
                            exit_cfg[k] for k in EXIT_PARAMS
                        )

                        m_tr = _metrics_for(parts["train"], arrays_1s, exit_cfg, fee_bps, conservative)
                        score = score_metrics(
                            m_tr,
                            objective,
                            min_trades=min_trades,
                            min_win_rate=min_win_rate,
                        )
                        if score > -1e17:
                            prev = best_by_fold.get(f)
                            if prev is None or score > prev["score"] + 1e-12:
                                best_by_fold[f] = {
                                    "score": score,
                                    "config": {**sk, **exit_cfg},
                                    "metrics": m_tr,
                                }

                        m_te = _metrics_for(parts["test"], arrays_1s, exit_cfg, fee_bps, conservative)
                        row = {**sk, **exit_cfg, "fold": f}
                        row.update({f"test_{k}": v for k, v in m_te.items()})
                        all_test_rows.append(row)

                        done += 1
                        pbar.update(1)

    pbar.close()

    # --- per-fold best (selected on train, reported on test) ---
    fold_rows = []
    for f, (tr_s, tr_e, te_s, te_e) in enumerate(splits, start=1):
        best = best_by_fold.get(f)
        header = {
            "fold": f,
            "train_start": tr_s,
            "train_end": tr_e,
            "test_start": te_s,
            "test_end": te_e,
        }
        if best is None:
            fold_rows.append(header)
            continue
        cfg_best = best["config"]
        m_tr = best["metrics"]
        part = None
        # Find the test metrics for this exact config and fold in all_test_rows.
        test_row = None
        for row in all_test_rows:
            if row["fold"] != f:
                continue
            if all(row[k] == cfg_best[k] for k in cfg_best):
                test_row = row
                break
        row = dict(header)
        row.update(cfg_best)
        row.update({f"train_{k}": v for k, v in m_tr.items()})
        if test_row is not None:
            row.update({k: v for k, v in test_row.items() if k.startswith("test_")})
        fold_rows.append(row)

    df_f = pd.DataFrame(fold_rows)
    if not df_f.empty:
        df_f.to_csv(outdir / "results_per_fold.csv", index=False)

    # --- aggregate test metrics of the per-fold best ---
    agg_rows = []
    if not df_f.empty:
        for c in [
            "test_net_pnl_pct",
            "test_num_trades",
            "test_win_rate",
            "test_profit_factor",
            "test_sharpe_trades",
            "test_max_drawdown_pct",
            "test_calmar",
            "test_time_in_pos_pct",
            "test_avg_hold_sec",
        ]:
            if c not in df_f:
                continue
            s = df_f[c].dropna()
            if len(s) == 0:
                continue
            agg_rows.append(
                {
                    "metric": c,
                    "median": float(s.median()),
                    "mean": float(s.mean()),
                    "std": float(s.std(ddof=1)) if len(s) > 1 else 0.0,
                    "min": float(s.min()),
                    "max": float(s.max()),
                    "pct_profitable_folds": float((s > 0).mean() * 100.0),
                }
            )
    df_agg = pd.DataFrame(agg_rows)
    if not df_agg.empty:
        df_agg.to_csv(outdir / "aggregate_test.csv", index=False)

    # --- top-10 configurations by median(test_sharpe) over all folds ---
    df_all = pd.DataFrame(all_test_rows)
    if not df_all.empty:
        df_all.to_csv(outdir / "all_configs_test_metrics.csv", index=False)
        group_cols = [k for k in SIGNAL_PARAMS + EXIT_PARAMS if k in df_all.columns]
        grouped = df_all.groupby(group_cols, dropna=False)
        summary = grouped.agg(
            n_folds=("test_sharpe_trades", "size"),
            median_test_sharpe=("test_sharpe_trades", "median"),
            mean_test_sharpe=("test_sharpe_trades", "mean"),
            std_test_sharpe=("test_sharpe_trades", "std"),
            median_test_net_pnl_pct=("test_net_pnl_pct", "median"),
            mean_test_net_pnl_pct=("test_net_pnl_pct", "mean"),
            median_test_win_rate=("test_win_rate", "median"),
            mean_test_win_rate=("test_win_rate", "mean"),
            median_test_num_trades=("test_num_trades", "median"),
            total_test_trades=("test_num_trades", "sum"),
            median_test_profit_factor=("test_profit_factor", "median"),
            median_test_max_dd_pct=("test_max_drawdown_pct", "median"),
            median_test_calmar=("test_calmar", "median"),
            median_test_time_in_pos_pct=("test_time_in_pos_pct", "median"),
            pct_profitable_folds=(
                "test_net_pnl_pct",
                lambda s: float((s > 0).mean() * 100.0),
            ),
        ).reset_index()
        summary = summary.sort_values(
            ["median_test_sharpe", "median_test_net_pnl_pct"],
            ascending=[False, False],
        )
        # A config that never trades on test is not interesting, even if
        # its median Sharpe is 0.
        summary = summary[summary["total_test_trades"] > 0]
        summary.to_csv(outdir / "all_configs_test_summary.csv", index=False)
        summary.head(10).to_csv(outdir / "top10_configs.csv", index=False)

    meta = {
        "n_folds": n_folds,
        "walk_forward_mode": wf_mode,
        "objective": objective,
        "train_days": int(train_days) if train_days is not None else None,
        "test_days": int(test_days) if test_days is not None else None,
        "fee_bps": fee_bps,
        "conservative_intrabar": conservative,
        "min_trades": min_trades,
        "min_win_rate": min_win_rate,
        "grid_size": len(grid),
        "signal_param_sets": len(signal_keys),
        "exit_param_sets": len(exit_keys),
        "folds_processed": len(splits),
        "data_rows_1s": int(len(df1)),
        "data_start": int(df1["ts"].min()),
        "data_end": int(df1["ts"].max()),
        "elapsed_sec": float(time.time() - t_start),
    }
    (outdir / "summary.json").write_text(json.dumps(meta, indent=2))

    return meta
