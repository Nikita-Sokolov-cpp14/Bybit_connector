# Metrics
from __future__ import annotations

import numpy as np
import pandas as pd

OBJECTIVE_ALIASES = {
    "sharpe": "sharpe_trades",
    "sharpe_trades": "sharpe_trades",
    "profit_factor": "profit_factor",
    "pf": "profit_factor",
    "net_pnl": "net_pnl_pct",
    "net_pnl_pct": "net_pnl_pct",
    "calmar": "calmar",
    "win_rate": "win_rate",
    "accuracy": "win_rate",
    "expectancy": "expectancy_pct",
}


def max_drawdown_from_trades(pnl_pct: np.ndarray) -> float:
    """Max drawdown of the closed-trade equity curve, starting from equity 0."""
    if len(pnl_pct) == 0:
        return 0.0
    eq = np.concatenate(([0.0], np.cumsum(np.asarray(pnl_pct, dtype=np.float64))))
    peak = np.maximum.accumulate(eq)
    dd = peak - eq
    return float(np.max(dd))


def time_in_position_sec(
    entry_ts: np.ndarray,
    exit_ts: np.ndarray,
    start_ts: int | None = None,
    end_ts: int | None = None,
) -> float:
    """Total seconds covered by trades, clipped to [start_ts, end_ts]."""
    if len(entry_ts) == 0:
        return 0.0
    lo = entry_ts.astype(np.int64, copy=True)
    hi = exit_ts.astype(np.int64, copy=True)
    if start_ts is not None:
        lo = np.maximum(lo, int(start_ts))
    if end_ts is not None:
        hi = np.minimum(hi, int(end_ts))
    overlap = np.maximum(0, hi - lo)
    return float(np.sum(overlap, dtype=np.int64) / 1000.0)


def compute_metrics(
    trades_df: pd.DataFrame,
    total_signals_buy: int = 0,
    total_signals_sell: int = 0,
    in_pos_sec_total: float = 0.0,
    total_sec: float = 0.0,
    fee_bps: float = 16.0,
) -> dict:
    """Per-fold metrics. Sharpe is per-trade: mean/std(ddof=1) * sqrt(N)."""
    empty = {
        "net_pnl_pct": 0.0,
        "gross_pnl_pct": 0.0,
        "num_trades": 0,
        "win_rate": 0.0,
        "gross_win_rate": 0.0,
        "profit_factor": 0.0,
        "sharpe_trades": 0.0,
        "max_drawdown_pct": 0.0,
        "avg_hold_sec": 0.0,
        "buy_signals": int(total_signals_buy),
        "sell_signals": int(total_signals_sell),
        "time_in_pos_pct": 0.0,
        "calmar": 0.0,
        "expectancy_pct": 0.0,
        "avg_win_pct": 0.0,
        "avg_loss_pct": 0.0,
        "largest_win_pct": 0.0,
        "largest_loss_pct": 0.0,
        "tp_count": 0,
        "sl_count": 0,
        "timeout_count": 0,
        "tp_rate": 0.0,
        "sl_rate": 0.0,
        "timeout_rate": 0.0,
        "long_trades": 0,
        "short_trades": 0,
    }
    if trades_df is None or trades_df.empty:
        return empty

    pnl = trades_df["pnl_pct"].to_numpy(dtype=np.float64)
    n = len(pnl)
    if n == 0:
        return empty

    gross_pnl = pnl + 2.0 * (fee_bps / 10000.0) * 100.0
    wins = pnl[pnl > 0.0]
    losses = pnl[pnl < 0.0]
    gross_wins = gross_pnl[gross_pnl > 0.0]

    net_pnl = float(np.sum(pnl))
    wr = float(len(wins) / n)
    gross_wr = float(len(gross_wins) / n)

    gp = float(np.sum(wins))
    gl = -float(np.sum(losses))
    if gl < 1e-12:
        pf = 999.0 if gp > 0 else 0.0
    else:
        pf = gp / gl

    if n <= 1 or np.std(pnl, ddof=1) <= 1e-12:
        sharpe_t = 0.0
    else:
        sharpe_t = float(np.mean(pnl) / np.std(pnl, ddof=1) * np.sqrt(n))

    mdd = max_drawdown_from_trades(pnl)

    avg_hold = float(trades_df["hold_sec"].mean()) if "hold_sec" in trades_df else 0.0

    in_pos_pct = 0.0
    if total_sec > 0:
        in_pos_pct = float(in_pos_sec_total * 100.0 / total_sec)

    calmar = float(net_pnl / mdd) if mdd > 1e-12 else 0.0
    expectancy = float(np.mean(pnl))

    reasons = (
        trades_df["exit_reason"].to_numpy()
        if "exit_reason" in trades_df
        else np.array([], dtype=object)
    )
    tp_count = int(np.sum(reasons == "tp")) if len(reasons) else 0
    sl_count = int(np.sum(reasons == "sl")) if len(reasons) else 0
    to_count = int(np.sum(reasons == "timeout")) if len(reasons) else 0

    sides = trades_df["side"].to_numpy() if "side" in trades_df else np.array([])
    long_n = int(np.sum(sides == 1)) if len(sides) else 0
    short_n = int(np.sum(sides == -1)) if len(sides) else 0

    return {
        "net_pnl_pct": net_pnl,
        "gross_pnl_pct": float(np.sum(gross_pnl)),
        "num_trades": int(n),
        "win_rate": wr,
        "gross_win_rate": gross_wr,
        "profit_factor": pf,
        "sharpe_trades": sharpe_t,
        "max_drawdown_pct": mdd,
        "avg_hold_sec": avg_hold,
        "buy_signals": int(total_signals_buy),
        "sell_signals": int(total_signals_sell),
        "time_in_pos_pct": in_pos_pct,
        "calmar": calmar,
        "expectancy_pct": expectancy,
        "avg_win_pct": float(np.mean(wins)) if len(wins) else 0.0,
        "avg_loss_pct": float(np.mean(losses)) if len(losses) else 0.0,
        "largest_win_pct": float(np.max(wins)) if len(wins) else 0.0,
        "largest_loss_pct": float(np.min(losses)) if len(losses) else 0.0,
        "tp_count": tp_count,
        "sl_count": sl_count,
        "timeout_count": to_count,
        "tp_rate": tp_count / n,
        "sl_rate": sl_count / n,
        "timeout_rate": to_count / n,
        "long_trades": long_n,
        "short_trades": short_n,
    }


def objective_value(metrics: dict, objective: str) -> float:
    """Map config objective name to a metric and return its value."""
    key = OBJECTIVE_ALIASES.get(str(objective).strip().lower(), "sharpe_trades")
    value = metrics.get(key, -1e18)
    if value is None or (isinstance(value, float) and not np.isfinite(value)):
        return -1e18
    return float(value)


def score_metrics(
    metrics: dict,
    objective: str,
    min_trades: int = 0,
    min_win_rate: float = 0.0,
) -> float:
    """Objective with optional trade-count / win-rate constraints."""
    if metrics.get("num_trades", 0) < int(min_trades):
        return -1e18
    if metrics.get("win_rate", 0.0) < float(min_win_rate):
        return -1e18
    return objective_value(metrics, objective)
