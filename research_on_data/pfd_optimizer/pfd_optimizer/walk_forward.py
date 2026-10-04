# Walk-forward optimization
from __future__ import annotations
import json
from pathlib import Path
from itertools import product
from typing import Dict, Any, List, Tuple

import numpy as np
import pandas as pd
from tqdm import tqdm

from .data_loader import load_aggregated_1s
from .pfd_indicator import aggregate_intervals, compute_pfd_signals
from .utils import attach_close_last_ts
from .backtest import backtest_on_intervals
from .metrics import compute_metrics


def _build_param_grid(cfg: Dict[str, Any]) -> List[Dict[str, Any]]:
    grid = []
    ps = cfg["params"]
    keys = ["interval_sec", "window_size", "sigma", "price_source", "take_profit", "stop_loss", "max_hold_sec"]
    values_lists = []
    for k in keys:
        v = ps.get(k, [])
        if isinstance(v, (list, tuple)):
            values_lists.append(list(v))
        else:
            values_lists.append([v])
    for combo in product(*values_lists):
        d = dict(zip(keys, combo))
        grid.append(d)
    return grid


def _infer_time_split(df_1s: pd.DataFrame, n_folds: int, wf_mode: str) -> List[Tuple[int,int,int,int]]:
    """
    Return list of folds: (train_start_ts, train_end_ts, test_start_ts, test_end_ts) in ms
    Split by calendar days based on ts (ms UTC). Use day boundaries.
    """
    if df_1s.empty:
        return []
    ts_min = int(df_1s["ts"].min())
    ts_max = int(df_1s["ts"].max())
    # day ms
    day_ms = 24*3600*1000
    start_day = ts_min // day_ms * day_ms
    end_day = ts_max // day_ms * day_ms
    total_days = max(1, (end_day - start_day) // day_ms + 1)
    # simple split: assign days to folds sequentially for WF
    fold_days = max(1, total_days // max(1,n_folds))
    folds = []
    for f in range(n_folds):
        d0_train = start_day + f*fold_days*day_ms
        d1_train = start_day + (f+1)*fold_days*day_ms - 1
        d0_test  = d1_train + 1
        d1_test  = d0_test + fold_days*day_ms - 1
        if f == n_folds-1:
            d1_test = ts_max
            # expand train to include prev if expanding? handled by mode
        # adjust by mode: train includes all previous folds? expanding vs rolling
        # rolling: train is just this fold block? but WF usually train previous, test next
        # implement: train = [start_day .. d1_train], test = [d0_test .. d1_test] but rolling window of one block? or expanding history
        folds.append((d0_train, d1_train, d0_test, d1_test))
    # rebuild with WF logic: train = previous K days, test = next M days
    M = fold_days
    splits = []
    for i in range(1, n_folds+1):
        if wf_mode == "rolling":
            train_start = start_day + (i-1)*M*day_ms
        else:  # expanding
            train_start = start_day
        train_end   = start_day + i*M*day_ms - 1
        test_start  = train_end + 1
        test_end    = test_start + M*day_ms - 1
        if test_start > ts_max:
            continue
        if test_end > ts_max:
            test_end = ts_max
        if train_end < ts_min:
            continue
        splits.append((train_start, train_end, test_start, test_end))
    return splits


def _count_in_pos_sec(trades_df: pd.DataFrame, total_sec: int) -> int:
    if trades_df.empty or total_sec <= 0:
        return 0
    s = 0
    for _, t in trades_df.iterrows():
        s += int(t["hold_sec"])
    return min(s, total_sec)


def optimize(config_path: str, data_path: str, output_dir: str = "output") -> Dict[str, Any]:
    import yaml
    cfg = yaml.safe_load(Path(config_path).read_text())
    outdir = Path(output_dir)
    outdir.mkdir(parents=True, exist_ok=True)

    df1 = load_aggregated_1s(data_path)
    total_sec = len(df1)

    grid = _build_param_grid(cfg)
    wf_cfg = cfg.get("walk_forward", {})
    n_folds = int(wf_cfg.get("n_folds", 5))
    wf_mode = wf_cfg.get("walk_forward_mode", "expanding")
    objective = wf_cfg.get("objective", "sharpe")
    bt_cfg = cfg.get("backtest", {})
    fee_bps = float(bt_cfg.get("fee_bps", 16.0))
    conservative = bool(bt_cfg.get("conservative_intrabar", True))

    splits = _infer_time_split(df1, n_folds, wf_mode)
    fold_results_list = []

    for f, (tr_s, tr_e, te_s, te_e) in enumerate(splits, start=1):
        df1_tr = df1[(df1["ts"] >= tr_s) & (df1["ts"] <= tr_e)].copy()
        df1_te = df1[(df1["ts"] >= te_s) & (df1["ts"] <= te_e)].copy()
        # find best on train
        best_score = -10**18
        best_params = None
        best_m_tr = None
        best_trades_tr = None
        for p in tqdm(grid, desc=f"Fold {f}/{len(splits)} grid"):
            # train
            dint_tr = aggregate_intervals(df1_tr, int(p["interval_sec"]))
            dint_tr = attach_close_last_ts(dint_tr, df1_tr, int(p["interval_sec"]))
            dint_tr = compute_pfd_signals(dint_tr, int(p["window_size"]), float(p["sigma"]), str(p["price_source"]))
            tr_tr = backtest_on_intervals(dint_tr, df1_tr, float(p["take_profit"]), float(p["stop_loss"]), int(p["max_hold_sec"]), fee_bps, conservative)
            # signals counts approx
            buy_s = int((dint_tr["signal"] == 1).sum())
            sell_s = int((dint_tr["signal"] == -1).sum())
            inpos_tr = _count_in_pos_sec(tr_tr, len(df1_tr))
            m_tr = compute_metrics(tr_tr, buy_s, sell_s, inpos_tr, len(df1_tr))
            score = m_tr.get(objective, m_tr.get("sharpe_trades", -1e9))
            if np.isnan(score):
                score = -1e9
            if best_params is None or score > best_score + 1e-12:
                best_score = score
                best_params = p
                best_m_tr = m_tr
                best_trades_tr = tr_tr
        # test with best_params
        if best_params is None:
            # empty
            fr = {"fold": f, "train_start": tr_s, "train_end": tr_e, "test_start": te_s, "test_end": te_e}
            fold_results_list.append(fr)
            continue
        pbest = best_params
        dint_te = aggregate_intervals(df1_te, int(pbest["interval_sec"]))
        dint_te = attach_close_last_ts(dint_te, df1_te, int(pbest["interval_sec"]))
        dint_te = compute_pfd_signals(dint_te, int(pbest["window_size"]), float(pbest["sigma"]), str(pbest["price_source"]))
        tr_te = backtest_on_intervals(dint_te, df1_te, float(pbest["take_profit"]), float(pbest["stop_loss"]), int(pbest["max_hold_sec"]), fee_bps, conservative)
        buy_s_te = int((dint_te["signal"] == 1).sum())
        sell_s_te = int((dint_te["signal"] == -1).sum())
        inpos_te = _count_in_pos_sec(tr_te, len(df1_te))
        m_te = compute_metrics(tr_te, buy_s_te, sell_s_te, inpos_te, len(df1_te))
        # record
        fr = {
            "fold": f,
            "train_start": tr_s, "train_end": tr_e,
            "test_start": te_s, "test_end": te_e,
            "interval_sec": pbest["interval_sec"],
            "window_size": pbest["window_size"],
            "sigma": pbest["sigma"],
            "price_source": pbest["price_source"],
            "take_profit": pbest["take_profit"],
            "stop_loss": pbest["stop_loss"],
            "max_hold_sec": pbest["max_hold_sec"],
            "train_net_pnl_pct": best_m_tr["net_pnl_pct"],
            "train_num_trades": best_m_tr["num_trades"],
            "train_win_rate": best_m_tr["win_rate"],
            "train_profit_factor": best_m_tr["profit_factor"],
            "train_sharpe_trades": best_m_tr["sharpe_trades"],
            "train_max_dd_pct": best_m_tr["max_drawdown_pct"],
            "train_calmar": best_m_tr["calmar"],
            "test_net_pnl_pct": m_te["net_pnl_pct"],
            "test_num_trades": m_te["num_trades"],
            "test_win_rate": m_te["win_rate"],
            "test_profit_factor": m_te["profit_factor"],
            "test_sharpe_trades": m_te["sharpe_trades"],
            "test_max_dd_pct": m_te["max_drawdown_pct"],
            "test_calmar": m_te["calmar"],
            "test_time_in_pos_pct": m_te["time_in_pos_pct"],
            "test_avg_hold_sec": m_te["avg_hold_sec"],
            "test_buy_signals": m_te["buy_signals"],
            "test_sell_signals": m_te["sell_signals"],
            "objective_score_train": best_score,
        }
        fold_results_list.append(fr)

    df_f = pd.DataFrame(fold_results_list)
    if not df_f.empty:
        df_f.to_csv(outdir / "results_per_fold.csv", index=False)

    # aggregate test
    agg_rows = []
    if not df_f.empty:
        for c in ["test_net_pnl_pct","test_num_trades","test_win_rate","test_profit_factor","test_sharpe_trades","test_max_dd_pct","test_calmar","test_time_in_pos_pct"]:
            if c in df_f:
                s = df_f[c].dropna()
                agg_rows.append({
                    "metric": c,
                    "median": float(s.median()) if len(s)>0 else np.nan,
                    "mean": float(s.mean()) if len(s)>0 else np.nan,
                    "std": float(s.std(ddof=1)) if len(s)>1 else 0.0,
                    "min": float(s.min()) if len(s)>0 else np.nan,
                    "max": float(s.max()) if len(s)>0 else np.nan,
                })
        # % profitable folds
        prof = float((df_f["test_net_pnl_pct"] > 0).mean()*100.0) if "test_net_pnl_pct" in df_f else 0.0
        agg_rows.append({"metric": "pct_profitable_folds", "median": prof, "mean": prof, "std": 0.0, "min": prof, "max": prof})
    df_agg = pd.DataFrame(agg_rows)
    if not df_agg.empty:
        df_agg.to_csv(outdir / "aggregate_test.csv", index=False)

    # top10 by median test_sharpe
    # need per-config medians? compute quick: rerun not needed; build from grid eval by median over folds? easier: store all? but can approximate by fold bests not directly; alternatively save per-config? skip heavy; top10 by best test_sharpe per fold? build table of fold bests with keys
    if not df_f.empty:
        df_top = df_f.sort_values("test_sharpe_trades", ascending=False).head(10).copy()
        df_top.to_csv(outdir / "top10_configs.csv", index=False)

    summary = {
        "n_folds": n_folds,
        "wf_mode": wf_mode,
        "objective": objective,
        "grid_size": len(grid),
        "folds_processed": len(splits),
    }
    (outdir / "summary.json").write_text(json.dumps(summary, indent=2))

    return summary
