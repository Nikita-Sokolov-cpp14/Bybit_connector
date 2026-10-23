# Backtest: event-driven with intrabar TP/SL and timeout (fast numpy implementation).
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np
import pandas as pd


@dataclass
class Trade:
    entry_ts: int
    exit_ts: int
    side: int
    entry_price: float
    exit_price: float
    pnl_pct: float
    hold_sec: int
    exit_reason: str


@dataclass
class BacktestResult:
    entry_ts: np.ndarray
    exit_ts: np.ndarray
    side: np.ndarray
    entry_price: np.ndarray
    exit_price: np.ndarray
    pnl_pct: np.ndarray
    hold_sec: np.ndarray
    exit_reason: np.ndarray

    def __len__(self) -> int:
        return len(self.pnl_pct)

    def to_frame(self) -> pd.DataFrame:
        return pd.DataFrame(
            {
                "entry_ts": self.entry_ts,
                "exit_ts": self.exit_ts,
                "side": self.side,
                "entry_price": self.entry_price,
                "exit_price": self.exit_price,
                "pnl_pct": self.pnl_pct,
                "hold_sec": self.hold_sec,
                "exit_reason": self.exit_reason,
            }
        )


EMPTY_RESULT = BacktestResult(
    np.empty(0, dtype=np.int64),
    np.empty(0, dtype=np.int64),
    np.empty(0, dtype=np.int8),
    np.empty(0, dtype=np.float64),
    np.empty(0, dtype=np.float64),
    np.empty(0, dtype=np.float64),
    np.empty(0, dtype=np.int64),
    np.empty(0, dtype=object),
)


def calc_pnl(entry_price: float, exit_price: float, side: int, fee_bps: float) -> float:
    """Net PnL in percent; the fee is applied to both entry and exit."""
    fee = fee_bps / 10000.0
    gross = side * (exit_price / entry_price - 1.0)
    return (gross - 2.0 * fee) * 100.0


def run_backtest(
    sig_ts: np.ndarray,
    sig_dir: np.ndarray,
    entry_px: np.ndarray,
    arrays_1s: dict[str, np.ndarray],
    take_profit: float,
    stop_loss: float,
    max_hold_sec: int,
    fee_bps: float = 16.0,
    conservative_intrabar: bool = True,
    start_ts: Optional[int] = None,
    end_ts: Optional[int] = None,
    exit_score_ts: Optional[np.ndarray] = None,
    exit_score: Optional[np.ndarray] = None,
    exit_score_px: Optional[np.ndarray] = None,
    exit_score_threshold: float = 0.0,
) -> BacktestResult:
    """
    Event-driven backtest.

    ``sig_ts`` is the close timestamp of the interval in which the signal was
    produced; ``entry_px`` is the close of that second (or the configured PFD
    price source). Exits are checked on every subsequent 1s bar by high/low.

    Rules:
    - only one position at a time; signals while a position is open are ignored;
    - TP/SL -> exit at the exact threshold price;
    - if TP and SL are touched in the same 1s bar, SL wins when
      ``conservative_intrabar`` is True;
    - timeout -> close at the last available 1s close.

    Optional early exit: ``exit_score`` is an interval score (e.g. the same
    comb or pz). For a long, exit if score <= -threshold; for a short, exit if
    score >= +threshold. The exit price is ``exit_score_px`` at that interval.
    TP/SL always win if they happen in the same or an earlier second.
    """
    n_signals = len(sig_dir)
    if n_signals == 0:
        return EMPTY_RESULT

    ts1 = arrays_1s["ts"]
    high1 = arrays_1s["high"]
    low1 = arrays_1s["low"]
    close1 = arrays_1s["close"]
    n1 = len(ts1)
    if n1 == 0:
        return EMPTY_RESULT

    if exit_score is not None:
        if exit_score_ts is None or exit_score_px is None:
            raise ValueError("exit_score_ts and exit_score_px are required with exit_score")
        exit_score_ts = np.asarray(exit_score_ts, dtype=np.int64)
        exit_score = np.asarray(exit_score, dtype=np.float64)
        exit_score_px = np.asarray(exit_score_px, dtype=np.float64)
        if not (len(exit_score_ts) == len(exit_score) == len(exit_score_px)):
            raise ValueError("exit score arrays must have the same length")

    fee = fee_bps / 10000.0
    hold_ms = int(max_hold_sec) * 1000

    out_entry_ts: list[int] = []
    out_exit_ts: list[int] = []
    out_side: list[int] = []
    out_entry_px: list[float] = []
    out_exit_px: list[float] = []
    out_pnl: list[float] = []
    out_hold: list[int] = []
    out_reason: list[str] = []

    last_exit_ts = -1

    for j in range(n_signals):
        direction = int(sig_dir[j])
        if direction == 0:
            continue
        e_ts = int(sig_ts[j])
        if start_ts is not None and e_ts < start_ts:
            continue
        if end_ts is not None and e_ts > end_ts:
            continue
        if e_ts < last_exit_ts:
            continue

        e_px = float(entry_px[j])
        if not np.isfinite(e_px) or e_px <= 0.0:
            continue

        # Bars strictly after the entry second.
        e0 = int(np.searchsorted(ts1, e_ts, side="right"))
        hold_end = e_ts + hold_ms
        scan_end = hold_end if end_ts is None else min(hold_end, int(end_ts))
        e1 = int(np.searchsorted(ts1, scan_end, side="right"))
        if e1 > n1:
            e1 = n1

        if e0 >= e1:
            # No forward bars inside the holding window (data gap / end of file).
            exit_ts = e_ts
            exit_px = e_px
            reason = "timeout"
            hold_sec = 0
        else:
            h = high1[e0:e1]
            l = low1[e0:e1]

            if direction == 1:
                tp_thr = e_px * (1.0 + take_profit)
                sl_thr = e_px * (1.0 - stop_loss)
                tp_hits = h >= tp_thr
                sl_hits = l <= sl_thr
            else:
                tp_thr = e_px * (1.0 - take_profit)
                sl_thr = e_px * (1.0 + stop_loss)
                tp_hits = l <= tp_thr
                sl_hits = h >= sl_thr

            i_tp = int(np.argmax(tp_hits))
            if not tp_hits[i_tp]:
                i_tp = -1
            i_sl = int(np.argmax(sl_hits))
            if not sl_hits[i_sl]:
                i_sl = -1

            if i_tp < 0 and i_sl < 0:
                last = e1 - 1
                exit_px = float(close1[last])
                # Timeout is at the holding limit unless the fold ends earlier.
                exit_ts = min(hold_end, scan_end)
                if exit_ts <= e_ts:
                    exit_ts = int(ts1[last]) + 1000
                reason = "timeout"
            elif i_sl < 0:
                k = e0 + i_tp
                exit_ts = int(ts1[k])
                exit_px = tp_thr
                reason = "tp"
            elif i_tp < 0:
                k = e0 + i_sl
                exit_ts = int(ts1[k])
                exit_px = sl_thr
                reason = "sl"
            elif i_sl < i_tp:
                k = e0 + i_sl
                exit_ts = int(ts1[k])
                exit_px = sl_thr
                reason = "sl"
            elif i_tp < i_sl:
                k = e0 + i_tp
                exit_ts = int(ts1[k])
                exit_px = tp_thr
                reason = "tp"
            else:
                # Same 1s bar: conservative -> stop-loss first.
                k = e0 + i_tp
                exit_ts = int(ts1[k])
                if conservative_intrabar:
                    exit_px = sl_thr
                    reason = "sl"
                else:
                    exit_px = tp_thr
                    reason = "tp"

            hold_sec = max(0, (exit_ts - e_ts) // 1000)

        # Optional early exit by an interval score (e.g. opposite PFD).
        # TP/SL wins ties because they are evaluated intrabar.
        if exit_score is not None:
            k0 = int(np.searchsorted(exit_score_ts, e_ts, side="right"))
            k1 = int(np.searchsorted(exit_score_ts, scan_end, side="right"))
            if k0 < k1:
                sc = exit_score[k0:k1]
                if direction == 1:
                    hits = np.isfinite(sc) & (sc <= -float(exit_score_threshold))
                else:
                    hits = np.isfinite(sc) & (sc >= float(exit_score_threshold))
                idx = int(np.argmax(hits))
                if hits[idx]:
                    exit_sig_ts = int(exit_score_ts[k0 + idx])
                    if exit_sig_ts < exit_ts:
                        exit_ts = exit_sig_ts
                        exit_px = float(exit_score_px[k0 + idx])
                        reason = "signal"
                        hold_sec = max(0, (exit_ts - e_ts) // 1000)

        gross = direction * (exit_px / e_px - 1.0)
        pnl_pct = (gross - 2.0 * fee) * 100.0

        out_entry_ts.append(e_ts)
        out_exit_ts.append(int(exit_ts))
        out_side.append(direction)
        out_entry_px.append(e_px)
        out_exit_px.append(float(exit_px))
        out_pnl.append(float(pnl_pct))
        out_hold.append(int(hold_sec))
        out_reason.append(reason)

        if exit_ts > last_exit_ts:
            last_exit_ts = int(exit_ts)

    if not out_pnl:
        return EMPTY_RESULT

    return BacktestResult(
        np.asarray(out_entry_ts, dtype=np.int64),
        np.asarray(out_exit_ts, dtype=np.int64),
        np.asarray(out_side, dtype=np.int8),
        np.asarray(out_entry_px, dtype=np.float64),
        np.asarray(out_exit_px, dtype=np.float64),
        np.asarray(out_pnl, dtype=np.float64),
        np.asarray(out_hold, dtype=np.int64),
        np.asarray(out_reason, dtype=object),
    )



def run_backtest_trailing(
    sig_ts: np.ndarray,
    sig_dir: np.ndarray,
    entry_px: np.ndarray,
    arrays_1s: dict[str, np.ndarray],
    take_profit: float,
    stop_loss: float,
    max_hold_sec: int,
    trail_distance: float,
    trail_activation: float = 0.0,
    fee_bps: float = 16.0,
    conservative_intrabar: bool = True,
    start_ts: Optional[int] = None,
    end_ts: Optional[int] = None,
) -> BacktestResult:
    """
    Event-driven backtest with a trailing stop.

    - initial stop is ``entry * (1 - stop_loss)`` for long and
      ``entry * (1 + stop_loss)`` for short;
    - after the price moves by ``trail_activation`` in favour of the position,
      the stop starts trailing the best high (long) / low (short) by
      ``trail_distance``;
    - the stop can only move in the favourable direction;
    - TP/SL are checked on each 1s bar using high/low; if both are touched in
      the same bar, the stop wins when ``conservative_intrabar`` is True;
    - timeout closes at the last available 1s close.

    This is a slower path than ``run_backtest`` because trailing is
    path-dependent, so it is intended for focused experiments rather than the
    full grid.
    """
    n_signals = len(sig_dir)
    if n_signals == 0:
        return EMPTY_RESULT

    ts1 = arrays_1s["ts"]
    high1 = arrays_1s["high"]
    low1 = arrays_1s["low"]
    close1 = arrays_1s["close"]
    n1 = len(ts1)
    if n1 == 0:
        return EMPTY_RESULT

    fee = fee_bps / 10000.0
    hold_ms = int(max_hold_sec) * 1000

    out_entry_ts: list[int] = []
    out_exit_ts: list[int] = []
    out_side: list[int] = []
    out_entry_px: list[float] = []
    out_exit_px: list[float] = []
    out_pnl: list[float] = []
    out_hold: list[int] = []
    out_reason: list[str] = []

    last_exit_ts = -1

    for j in range(n_signals):
        direction = int(sig_dir[j])
        if direction == 0:
            continue
        e_ts = int(sig_ts[j])
        if start_ts is not None and e_ts < start_ts:
            continue
        if end_ts is not None and e_ts > end_ts:
            continue
        if e_ts < last_exit_ts:
            continue

        e_px = float(entry_px[j])
        if not np.isfinite(e_px) or e_px <= 0.0:
            continue

        e0 = int(np.searchsorted(ts1, e_ts, side="right"))
        hold_end = e_ts + hold_ms
        scan_end = hold_end if end_ts is None else min(hold_end, int(end_ts))
        e1 = int(np.searchsorted(ts1, scan_end, side="right"))
        if e1 > n1:
            e1 = n1

        if direction == 1:
            initial_stop = e_px * (1.0 - stop_loss)
            stop = initial_stop
            best = e_px
            tp_thr = e_px * (1.0 + take_profit)
            activation_px = e_px * (1.0 + trail_activation)
        else:
            initial_stop = e_px * (1.0 + stop_loss)
            stop = initial_stop
            best = e_px
            tp_thr = e_px * (1.0 - take_profit)
            activation_px = e_px * (1.0 - trail_activation)

        exit_ts: int | None = None
        exit_px: float | None = None
        reason: str | None = None

        for k in range(e0, e1):
            h = float(high1[k])
            l = float(low1[k])
            hit_stop = (l <= stop) if direction == 1 else (h >= stop)
            hit_tp = (h >= tp_thr) if direction == 1 else (l <= tp_thr)

            if hit_stop and hit_tp:
                if conservative_intrabar:
                    exit_ts = int(ts1[k])
                    exit_px = stop
                    reason = "trail" if stop != initial_stop else "sl"
                else:
                    exit_ts = int(ts1[k])
                    exit_px = tp_thr
                    reason = "tp"
                break
            if hit_stop:
                exit_ts = int(ts1[k])
                exit_px = stop
                reason = "trail" if stop != initial_stop else "sl"
                break
            if hit_tp:
                exit_ts = int(ts1[k])
                exit_px = tp_thr
                reason = "tp"
                break

            if direction == 1:
                if h > best:
                    best = h
                if best >= activation_px:
                    candidate = best * (1.0 - trail_distance)
                    if candidate > stop:
                        stop = candidate
            else:
                if l < best:
                    best = l
                if best <= activation_px:
                    candidate = best * (1.0 + trail_distance)
                    if candidate < stop:
                        stop = candidate

        if exit_ts is None:
            if e1 > e0:
                last = e1 - 1
                exit_px = float(close1[last])
                exit_ts = min(hold_end, scan_end)
                if exit_ts <= e_ts:
                    exit_ts = int(ts1[last]) + 1000
            else:
                exit_px = e_px
                exit_ts = e_ts
            reason = "timeout"

        hold_sec = max(0, (int(exit_ts) - e_ts) // 1000)
        gross = direction * (float(exit_px) / e_px - 1.0)
        pnl_pct = (gross - 2.0 * fee) * 100.0

        out_entry_ts.append(e_ts)
        out_exit_ts.append(int(exit_ts))
        out_side.append(direction)
        out_entry_px.append(e_px)
        out_exit_px.append(float(exit_px))
        out_pnl.append(float(pnl_pct))
        out_hold.append(int(hold_sec))
        out_reason.append(str(reason))

        if exit_ts > last_exit_ts:
            last_exit_ts = int(exit_ts)

    if not out_pnl:
        return EMPTY_RESULT

    return BacktestResult(
        np.asarray(out_entry_ts, dtype=np.int64),
        np.asarray(out_exit_ts, dtype=np.int64),
        np.asarray(out_side, dtype=np.int8),
        np.asarray(out_entry_px, dtype=np.float64),
        np.asarray(out_exit_px, dtype=np.float64),
        np.asarray(out_pnl, dtype=np.float64),
        np.asarray(out_hold, dtype=np.int64),
        np.asarray(out_reason, dtype=object),
    )


def backtest_on_intervals(
    df_intervals: pd.DataFrame,
    df_1s: pd.DataFrame,
    take_profit: float,
    stop_loss: float,
    max_hold_sec: int,
    fee_bps: float = 16.0,
    conservative_intrabar: bool = True,
    start_ts: Optional[int] = None,
    end_ts: Optional[int] = None,
) -> pd.DataFrame:
    """Compatibility wrapper returning a trades DataFrame."""
    arrays = {
        "ts": df_1s["ts"].to_numpy(dtype=np.int64, copy=False),
        "high": df_1s["high"].to_numpy(dtype=np.float64, copy=False),
        "low": df_1s["low"].to_numpy(dtype=np.float64, copy=False),
        "close": df_1s["close"].to_numpy(dtype=np.float64, copy=False),
    }
    sig = df_intervals["signal"].to_numpy(dtype=np.int8, copy=False)
    ts = df_intervals["close_last_ts"].to_numpy(dtype=np.int64, copy=False)
    px = df_intervals["close_last"].to_numpy(dtype=np.float64, copy=False)
    res = run_backtest(
        ts,
        sig,
        px,
        arrays,
        take_profit,
        stop_loss,
        max_hold_sec,
        fee_bps,
        conservative_intrabar,
        start_ts,
        end_ts,
    )
    return res.to_frame()
