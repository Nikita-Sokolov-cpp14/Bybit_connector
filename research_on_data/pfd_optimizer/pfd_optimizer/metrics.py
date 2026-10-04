# Metrics
import numpy as np
import pandas as pd


def max_drawdown(equity_pct: np.ndarray) -> float:
    if len(equity_pct) == 0:
        return 0.0
    eq = np.cumsum(equity_pct)
    peak = -np.inf
    mdd = 0.0
    for v in eq:
        if v > peak:
            peak = v
        dd = (peak - v) if peak > 0 else 0.0
        if dd > mdd:
            mdd = dd
    # if peak <=0, return max drop from start
    if peak <= 0:
        mdd = max(0.0, -eq[-1]) if len(eq)>0 else 0.0
    return float(mdd)


def max_drawdown_from_trades(pnl_pct: np.ndarray) -> float:
    if len(pnl_pct) == 0:
        return 0.0
    eq = np.cumsum(pnl_pct)
    peak = eq[0] if len(eq)>0 else 0.0
    mdd = 0.0
    for v in eq:
        if v > peak:
            peak = v
        dd = peak - v
        if dd > mdd:
            mdd = dd
    return float(mdd)


def compute_metrics(trades_df: pd.DataFrame, total_signals_buy: int = 0, total_signals_sell: int = 0, in_pos_sec_total: int = 0, total_sec: int = 0) -> dict:
    if trades_df.empty:
        m = {
            "net_pnl_pct": 0.0,
            "num_trades": 0,
            "win_rate": 0.0,
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
        }
        return m

    pnl = trades_df["pnl_pct"].values.astype(float)
    net_pnl = float(np.sum(pnl))
    n = len(pnl)
    wins = pnl[pnl > 0.0]
    losses = pnl[pnl < 0.0]
    wr = float(len(wins) / n) if n > 0 else 0.0
    pf = 0.0
    gp = float(np.sum(wins)) if len(wins)>0 else 0.0
    gl = -float(np.sum(losses)) if len(losses)>0 else 0.0
    if gl < 1e-12:
        pf = 999.0 if gp > 0 else 0.0
    else:
        pf = gp / gl

    # sharpe per trades: mean/std * sqrt(n)
    if n <= 1 or np.std(pnl, ddof=1) < 1e-12:
        sharpe_t = 0.0
    else:
        sharpe_t = float(np.mean(pnl) / np.std(pnl, ddof=1) * np.sqrt(n))

    mdd = max_drawdown_from_trades(pnl)
    avg_hold = float(trades_df["hold_sec"].mean()) if "hold_sec" in trades_df else 0.0

    in_pos_pct = 0.0
    if total_sec > 0:
        in_pos_pct = float(in_pos_sec_total * 100.0 / total_sec)

    calmar = 0.0
    if mdd > 1e-12:
        calmar = net_pnl / mdd

    expectancy = float(np.mean(pnl)) if n>0 else 0.0
    avg_win = float(np.mean(wins)) if len(wins)>0 else 0.0
    avg_loss = float(np.mean(losses)) if len(losses)>0 else 0.0
    lw = float(np.max(wins)) if len(wins)>0 else 0.0
    ll = float(np.min(losses)) if len(losses)>0 else 0.0

    return {
        "net_pnl_pct": net_pnl,
        "num_trades": int(n),
        "win_rate": wr,
        "profit_factor": pf,
        "sharpe_trades": sharpe_t,
        "max_drawdown_pct": mdd,
        "avg_hold_sec": avg_hold,
        "buy_signals": int(total_signals_buy),
        "sell_signals": int(total_signals_sell),
        "time_in_pos_pct": in_pos_pct,
        "calmar": calmar,
        "expectancy_pct": expectancy,
        "avg_win_pct": avg_win,
        "avg_loss_pct": avg_loss,
        "largest_win_pct": lw,
        "largest_loss_pct": ll,
    }
