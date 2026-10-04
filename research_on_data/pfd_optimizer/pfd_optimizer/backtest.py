# Backtest: event-driven with intrabar TP/SL and timeout
import numpy as np
import pandas as pd
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class Trade:
    entry_ts: int
    exit_ts: int
    side: int  # +1 long, -1 short
    entry_price: float
    exit_price: float
    pnl_pct: float  # with fees
    hold_sec: int
    exit_reason: str  # "tp", "sl", "timeout"


def backtest_on_intervals(
    df_intervals: pd.DataFrame,
    df_1s: pd.DataFrame,
    take_profit: float,
    stop_loss: float,
    max_hold_sec: int,
    fee_bps: float = 16.0,
    conservative_intrabar: bool = True,
) -> pd.DataFrame:
    """
    Event-driven backtest:
    - one position at a time; new signals ignored if in position
    - entry on signal: entry price = close of the interval (or df_intervals close_interval/close_last)
    - exit on first of: TP, SL, timeout (max_hold_sec)
    - intrabar check by high/low each second of holding; if TP and SL hit in same second -> SL first if conservative_intrabar
    - fees apply to both sides (taker+slippage) at fee_bps (0.16% default)
    """
    trades = []
    if df_intervals.empty or df_1s.empty:
        return pd.DataFrame(trades)

    # Build 1s lookup helpers
    df1 = df_1s.copy()
    df1 = df1.sort_values("ts").reset_index(drop=True)
    # index by ts for fast lookup not strictly needed; iterate by ranges

    pos = 0  # 0 flat, +1 long, -1 short
    entry_ts = None
    entry_price = None
    entry_interval_idx = -1

    n_int = len(df_intervals)
    for i in range(n_int):
        row = df_intervals.iloc[i]
        sig = int(row["signal"])
        iv_start = int(row["ts_interval"])
        iv_close_ts = int(row["close_last_ts"]) if "close_last_ts" in row else None

        # If no position, check signal
        if pos == 0 and sig != 0:
            # enter at next logic: entry price is close of the interval in which signal occurs
            # signal is computed at interval i; entry happens at/using interval close context
            entry_price = float(row["close_interval"]) if "close_interval" in row else float(row["close_last"])
            entry_ts = int(row["close_last_ts"]) if "close_last_ts" in row else iv_start + int(row.get("count_sec", 1)) * 1000 - 1000
            # if close_last_ts missing, infer from 1s after iv_start? but we have df1
            pos = sig
            entry_interval_idx = i
            continue

        # If in position, monitor exit conditions starting from after entry? 
        # we entered at entry_ts; check each second from entry_ts onward until exit
        if pos != 0:
            # find exit: scan forward seconds from entry_ts+1? or from entry_ts to entry_ts+max_hold_sec
            # entry is at entry_ts (close of signal interval). Check intrabar from entry_ts? 
            # "by high/low of each second of holding" - include seconds after entry
            start_scan = entry_ts  # entry time point reference; check seconds after entry
            end_scan = entry_ts + max_hold_sec * 1000
            # scan all 1s with ts > entry_ts and ts <= end_scan? and also check the entry second? typically entry at close of that second
            # find candidate exit in df1 between start_scan and end_scan
            mask = (df1["ts"] > start_scan) & (df1["ts"] <= end_scan)
            candidates = df1[mask].copy()
            if len(candidates) == 0:
                # check timeout at end_scan
                # get price at end_scan? not directly available; use last available before or at end
                exit_price = float(df_intervals.iloc[i]["close_interval"]) if i < n_int else entry_price
                exit_ts = end_scan
                pnl_pct = calc_pnl(entry_price, exit_price, pos, fee_bps)
                trades.append(Trade(entry_ts, exit_ts, pos, entry_price, exit_price, pnl_pct, (exit_ts-entry_ts)//1000, "timeout"))
                pos = 0
                continue

            # check intrabar TP/SL in each second: examine high/low
            hit_sl = False
            hit_tp = False
            sl_ts = None
            sl_px = None
            tp_ts = None
            tp_px = None

            for _, srow in candidates.iterrows():
                h = float(srow["high"])
                l = float(srow["low"])
                if pos == 1:
                    # long: TP if high >= entry_price*(1+TP); SL if low <= entry_price*(1-SL)
                    if h >= entry_price * (1.0 + take_profit):
                        # tp hit
                        if not hit_tp or int(srow["ts"]) < tp_ts:
                            tp_ts = int(srow["ts"])
                            tp_px = entry_price * (1.0 + take_profit)
                    if l <= entry_price * (1.0 - stop_loss):
                        if not hit_sl or int(srow["ts"]) < sl_ts:
                            sl_ts = int(srow["ts"])
                            sl_px = entry_price * (1.0 - stop_loss)
                else:  # short
                    if l <= entry_price * (1.0 - take_profit):  # tp for short: price down
                        if not hit_tp or int(srow["ts"]) < tp_ts:
                            tp_ts = int(srow["ts"])
                            tp_px = entry_price * (1.0 - take_profit)
                    if h >= entry_price * (1.0 + stop_loss):
                        if not hit_sl or int(srow["ts"]) < sl_ts:
                            sl_ts = int(srow["ts"])
                            sl_px = entry_price * (1.0 + stop_loss)

            # determine first exit
            if hit_sl or hit_tp:
                if not hit_sl:
                    # tp only
                    exit_ts, exit_px, reason = tp_ts, tp_px, "tp"
                elif not hit_tp:
                    exit_ts, exit_px, reason = sl_ts, sl_px, "sl"
                else:
                    # both hit - first by time
                    if sl_ts < tp_ts:
                        exit_ts, exit_px, reason = sl_ts, sl_px, "sl"
                    elif tp_ts < sl_ts:
                        exit_ts, exit_px, reason = tp_ts, tp_px, "tp"
                    else:
                        # same second - conservative -> SL first
                        if conservative_intrabar:
                            exit_ts, exit_px, reason = sl_ts, sl_px, "sl"
                        else:
                            exit_ts, exit_px, reason = tp_ts, tp_px, "tp"
                pnl_pct = calc_pnl(entry_price, exit_px, pos, fee_bps)
                hold_sec = (exit_ts - entry_ts) // 1000
                trades.append(Trade(entry_ts, exit_ts, pos, entry_price, exit_px, pnl_pct, hold_sec, reason))
                pos = 0
                continue
            else:
                # timeout
                exit_ts = end_scan
                exit_px = float(candidates.iloc[-1]["close"]) if len(candidates) > 0 else entry_price
                # better use last close in range
                exit_px = float(candidates["close"].iloc[-1])
                pnl_pct = calc_pnl(entry_price, exit_px, pos, fee_bps)
                hold_sec = max_hold_sec
                trades.append(Trade(entry_ts, exit_ts, pos, entry_price, exit_px, pnl_pct, hold_sec, "timeout"))
                pos = 0
                continue

    return pd.DataFrame([t.__dict__ for t in trades])


def calc_pnl(entry_price: float, exit_price: float, side: int, fee_bps: float) -> float:
    # fees apply to both sides
    fee = fee_bps / 10000.0
    if side == 1:  # long
        gross = exit_price / entry_price - 1.0
    else:  # short
        gross = entry_price / exit_price - 1.0
    net = gross - 2.0 * fee
    return net * 100.0  # percent
