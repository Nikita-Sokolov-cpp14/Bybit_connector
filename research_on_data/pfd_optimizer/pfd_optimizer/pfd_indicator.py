# PFD indicator calculation matching C++ logic (vectorised).
from __future__ import annotations

import numpy as np
import pandas as pd


def aggregate_intervals(df_1s: pd.DataFrame, interval_sec: int) -> pd.DataFrame:
    """
    Aggregate 1s data into buckets of ``interval_sec`` seconds.

    Columns of the result:
    - ts_interval: bucket start, ms UTC
    - buy_vol_acc, sell_vol_acc: sums over the bucket
    - flow: (buy-sell)/(buy+sell) if sum > 1e-12 else 0
    - high_max, low_min: extremes inside the bucket
    - open_first, close_last, vwap_last: first open / last close / last vwap
    - close_last_ts: ts of the last 1s bar in the bucket (entry timestamp)
    - count_sec: number of 1s bars in the bucket (data gaps are not filled)
    """
    if interval_sec <= 0:
        raise ValueError("interval_sec must be positive")
    if df_1s.empty:
        return pd.DataFrame(
            columns=[
                "ts_interval",
                "buy_vol_acc",
                "sell_vol_acc",
                "flow",
                "high_max",
                "low_min",
                "open_first",
                "close_last",
                "vwap_last",
                "close_last_ts",
                "count_sec",
            ]
        )

    iv_ms = int(interval_sec) * 1000
    d = df_1s[["ts", "open", "high", "low", "close", "vwap", "buy_vol", "sell_vol"]].copy()
    d["ts_interval"] = (d["ts"] // iv_ms) * iv_ms

    grouped = d.groupby("ts_interval", sort=True).agg(
        buy_vol_acc=("buy_vol", "sum"),
        sell_vol_acc=("sell_vol", "sum"),
        high_max=("high", "max"),
        low_min=("low", "min"),
        open_first=("open", "first"),
        close_last=("close", "last"),
        vwap_last=("vwap", "last"),
        close_last_ts=("ts", "last"),
        count_sec=("ts", "size"),
    )
    grouped = grouped.reset_index()

    buy = grouped["buy_vol_acc"].to_numpy(dtype=np.float64)
    sell = grouped["sell_vol_acc"].to_numpy(dtype=np.float64)
    total = buy + sell
    flow = np.zeros_like(total)
    np.divide(buy - sell, total, out=flow, where=total > 1e-12)
    grouped["flow"] = flow

    # A stable, predictable column order.
    return grouped[
        [
            "ts_interval",
            "buy_vol_acc",
            "sell_vol_acc",
            "flow",
            "high_max",
            "low_min",
            "open_first",
            "close_last",
            "vwap_last",
            "close_last_ts",
            "count_sec",
        ]
    ]


def _rolling_mean_std(values: np.ndarray, window: int) -> tuple[np.ndarray, np.ndarray]:
    """Rolling mean and sample std (ddof=1) with ``window`` observations."""
    s = pd.Series(values)
    roll = s.rolling(window=window, min_periods=window)
    mean = roll.mean().to_numpy(dtype=np.float64)
    std = roll.std(ddof=1).to_numpy(dtype=np.float64)
    return mean, std


def compute_pfd_components(
    df_intervals: pd.DataFrame,
    window_size: int,
    price_source: str = "vwap",
) -> dict[str, np.ndarray]:
    """
    Compute the PFD components exactly as in the C++ implementation.

    For interval i >= W:
      tr       = p[i] / p[i-W] - 1
      rets     = p[j] / p[j-1] - 1, j = i-W+1 .. i        (W values)
      sig_px   = std(rets, ddof=1) * sqrt(W)
      mean_f   = mean(flow[i-W:i])                          (last W values)
      std_f    = std(flow[i-W:i], ddof=1)
      pz       = tr / sig_px
      intz     = mean_f * sqrt(W) / std_f
      comb     = pz - intz

    Intervals before the buffers are full get comb = 0.
    Guard: sig_px < 1e-9 or std_f < 1e-9 -> comb = 0.
    """
    W = int(window_size)
    if W < 2:
        raise ValueError("window_size must be >= 2")

    out = df_intervals
    n = len(out)
    if price_source == "close":
        prices = out["close_last"].to_numpy(dtype=np.float64)
    elif price_source == "vwap":
        prices = out["vwap_last"].to_numpy(dtype=np.float64)
    else:
        raise ValueError("price_source must be 'vwap' or 'close'")

    flow = out["flow"].to_numpy(dtype=np.float64)

    tr = np.zeros(n, dtype=np.float64)
    pz = np.zeros(n, dtype=np.float64)
    intz = np.zeros(n, dtype=np.float64)
    sig_px = np.zeros(n, dtype=np.float64)
    std_flow = np.zeros(n, dtype=np.float64)
    mean_flow = np.zeros(n, dtype=np.float64)
    valid = np.zeros(n, dtype=bool)

    if n <= W:
        return {
            "tr": tr,
            "pz": pz,
            "intz": intz,
            "sig_px": sig_px,
            "std_flow": std_flow,
            "mean_flow": mean_flow,
            "valid": valid,
            "comb": np.zeros(n, dtype=np.float64),
        }

    # rets[k] is the return ending at interval k+1.
    rets = prices[1:] / prices[:-1] - 1.0
    ret_mean, ret_std = _rolling_mean_std(rets, W)
    flow_mean, flow_std = _rolling_mean_std(flow, W)

    # For interval i the rolling window ends at index i-1.
    idx = np.arange(W, n)
    k = idx - 1

    tr[idx] = prices[idx] / prices[idx - W] - 1.0
    mean_flow[idx] = flow_mean[k]
    std_flow[idx] = flow_std[k]
    sig_px[idx] = ret_std[k] * np.sqrt(float(W))

    good_px = np.isfinite(sig_px[idx]) & (sig_px[idx] >= 1e-9)
    good_flow = np.isfinite(std_flow[idx]) & (std_flow[idx] >= 1e-9)
    px_idx = idx[good_px]
    flow_idx = idx[good_flow]

    pz[px_idx] = tr[px_idx] / sig_px[px_idx]
    intz[flow_idx] = mean_flow[flow_idx] * np.sqrt(float(W)) / std_flow[flow_idx]

    # `valid` keeps the original C++ guard (both price and flow defined).
    both_idx = idx[good_px & good_flow]
    valid[both_idx] = True

    comb = np.zeros(n, dtype=np.float64)
    comb[both_idx] = pz[both_idx] - intz[both_idx]

    return {
        "tr": tr,
        "pz": pz,
        "intz": intz,
        "sig_px": sig_px,
        "std_flow": std_flow,
        "mean_flow": mean_flow,
        "valid": valid,
        "comb": comb,
    }


def _ema(values: np.ndarray, span: int) -> np.ndarray:
    return pd.Series(values).ewm(span=span, adjust=False).mean().to_numpy(dtype=np.float64)


def compute_regime_features(
    df_intervals: pd.DataFrame,
    comp: dict[str, np.ndarray],
    price_source: str = "vwap",
    autocorr_window: int = 0,
    vol_window: int = 0,
) -> dict[str, np.ndarray]:
    """
    Causal regime features used by the optional filters.

    - autocorr: rolling lag-1 autocorrelation of interval returns.
      Positive -> trending/continuation regime, negative -> mean-reverting.
    - vol_ratio: current PFD price volatility divided by its rolling median.
      <1 = quiet regime, >1 = volatile regime.
    """
    out: dict[str, np.ndarray] = {}
    n = len(df_intervals)

    if autocorr_window and autocorr_window > 2:
        prices = (
            df_intervals["vwap_last"].to_numpy(dtype=np.float64)
            if price_source == "vwap"
            else df_intervals["close_last"].to_numpy(dtype=np.float64)
        )
        r = pd.Series(prices).pct_change()
        y = r.shift(1)
        w = int(autocorr_window)
        minp = w
        mx = r.rolling(w, min_periods=minp).mean()
        my = y.rolling(w, min_periods=minp).mean()
        cov = (r * y).rolling(w, min_periods=minp).mean() - mx * my
        vx = r.rolling(w, min_periods=minp).var(ddof=0)
        vy = y.rolling(w, min_periods=minp).var(ddof=0)
        denom = np.sqrt(vx * vy)
        ac = cov / denom
        ac = ac.replace([np.inf, -np.inf], np.nan).to_numpy(dtype=np.float64)
        out["autocorr"] = ac

    if vol_window and vol_window > 2:
        vol = np.asarray(comp["sig_px"], dtype=np.float64)
        w = int(vol_window)
        med = pd.Series(vol).rolling(w, min_periods=w).median().to_numpy()
        ratio = np.full(n, np.nan, dtype=np.float64)
        np.divide(vol, med, out=ratio, where=med > 0)
        out["vol_ratio"] = ratio

    return out


def compute_signals_from_components(
    df_intervals: pd.DataFrame,
    comp: dict[str, np.ndarray],
    sigma: float,
    signal_mode: str = "momentum",
    confirm_intervals: int = 0,
    trend_ema_span: int = 0,
    trend_filter_mode: str = "aligned",
    direction: str = "both",
    pz_weight: float = 1.0,
    intz_weight: float = 1.0,
    trend_prices: np.ndarray | None = None,
    trend_strength_min: float = 0.0,
    autocorr_window: int = 0,
    autocorr_mode: str = "follow",
    autocorr_threshold: float = 0.0,
    autocorr: np.ndarray | None = None,
    vol_window: int = 0,
    vol_ratio_min: float = 0.0,
    vol_ratio_max: float = 1e9,
    vol_ratio: np.ndarray | None = None,
) -> np.ndarray:
    """
    Turn the PFD score into +1/-1/0 signals.

    - momentum: comb >= +sigma -> Buy, comb <= -sigma -> Sell (C++ / spec).
    - reversal: the opposite interpretation (classic divergence).
    - confirm_intervals: require N previous signals of the same direction.
    - trend_ema_span: optional EMA trend filter.
    - trend_filter_mode: "aligned" (Buy above EMA / Sell below EMA) or
      "against" (Buy below EMA / Sell above EMA, i.e. counter-trend).
    - direction: "both", "long" or "short".
    """
    # Generalized score: pz_weight * pz - intz_weight * intz.
    # intz_weight = 0 gives pure price momentum (pz), which is also useful.
    comb = float(pz_weight) * comp["pz"] - float(intz_weight) * comp["intz"]
    valid = np.isfinite(comp["sig_px"]) & (comp["sig_px"] >= 1e-9)
    if float(intz_weight) != 0.0:
        valid = valid & np.isfinite(comp["std_flow"]) & (comp["std_flow"] >= 1e-9)
    valid = valid & np.isfinite(comb)
    raw = np.zeros(len(comb), dtype=np.int8)
    raw[valid & (comb >= sigma)] = 1
    raw[valid & (comb <= -sigma)] = -1

    if signal_mode == "reversal":
        raw = -raw
    elif signal_mode != "momentum":
        raise ValueError("signal_mode must be 'momentum' or 'reversal'")

    signal = raw

    if confirm_intervals and confirm_intervals > 0:
        confirmed = np.zeros_like(signal)
        n = len(signal)
        for i in range(confirm_intervals, n):
            if signal[i] == 0:
                continue
            ok = True
            for j in range(1, confirm_intervals + 1):
                if signal[i - j] != signal[i]:
                    ok = False
                    break
            if ok:
                confirmed[i] = signal[i]
        signal = confirmed

    if trend_ema_span and trend_ema_span > 1:
        if trend_prices is not None:
            prices = np.asarray(trend_prices, dtype=np.float64)
        elif df_intervals is None or df_intervals.empty:
            return signal
        else:
            prices = df_intervals["close_last"].to_numpy(dtype=np.float64)
        ema = _ema(prices, int(trend_ema_span))
        signal = signal.copy()
        if trend_filter_mode == "aligned":
            signal[(signal == 1) & (prices <= ema)] = 0
            signal[(signal == -1) & (prices >= ema)] = 0
        elif trend_filter_mode == "against":
            signal[(signal == 1) & (prices >= ema)] = 0
            signal[(signal == -1) & (prices <= ema)] = 0
        else:
            raise ValueError("trend_filter_mode must be 'aligned' or 'against'")

        if trend_strength_min and trend_strength_min > 0:
            m = float(trend_strength_min)
            signal = signal.copy()
            if trend_filter_mode == "aligned":
                signal[(signal == 1) & (prices < ema * (1.0 + m))] = 0
                signal[(signal == -1) & (prices > ema * (1.0 - m))] = 0
            else:
                signal[(signal == 1) & (prices > ema * (1.0 - m))] = 0
                signal[(signal == -1) & (prices < ema * (1.0 + m))] = 0

    if autocorr_window and autocorr_window > 0:
        if autocorr is None:
            raise ValueError("autocorr array is required when autocorr_window > 0")
        ac = np.asarray(autocorr, dtype=np.float64)
        signal = signal.copy()
        if autocorr_mode == "follow":
            keep = np.isfinite(ac) & (ac >= float(autocorr_threshold))
            signal[~keep] = 0
        elif autocorr_mode == "fade":
            keep = np.isfinite(ac) & (ac <= -float(autocorr_threshold))
            flip = keep & (signal != 0)
            signal[flip] = -signal[flip]
            signal[~keep] = 0
        else:
            raise ValueError("autocorr_mode must be 'follow' or 'fade'")

    if vol_window and vol_window > 0:
        if vol_ratio is None:
            raise ValueError("vol_ratio array is required when vol_window > 0")
        vr = np.asarray(vol_ratio, dtype=np.float64)
        signal = signal.copy()
        keep = np.isfinite(vr) & (vr >= float(vol_ratio_min)) & (vr <= float(vol_ratio_max))
        signal[~keep] = 0

    if direction == "long":
        signal = signal.copy()
        signal[signal < 0] = 0
    elif direction == "short":
        signal = signal.copy()
        signal[signal > 0] = 0
    elif direction != "both":
        raise ValueError("direction must be 'both', 'long' or 'short'")

    return signal


def compute_pfd_signals(
    df_intervals: pd.DataFrame,
    window_size: int,
    sigma: float,
    price_source: str = "vwap",
    signal_mode: str = "momentum",
    confirm_intervals: int = 0,
    trend_ema_span: int = 0,
    trend_filter_mode: str = "aligned",
    direction: str = "both",
    pz_weight: float = 1.0,
    intz_weight: float = 1.0,
    trend_strength_min: float = 0.0,
    autocorr_window: int = 0,
    autocorr_mode: str = "follow",
    autocorr_threshold: float = 0.0,
    vol_window: int = 0,
    vol_ratio_min: float = 0.0,
    vol_ratio_max: float = 1e9,
) -> pd.DataFrame:
    """Compatibility wrapper: returns ``df_intervals`` with PFD columns + signal."""
    out = df_intervals.copy()
    comp = compute_pfd_components(out, window_size, price_source)
    out["tr"] = comp["tr"]
    out["pz"] = comp["pz"]
    out["intz"] = comp["intz"]
    out["sig_px"] = comp["sig_px"]
    out["std_flow"] = comp["std_flow"]
    out["comb"] = float(pz_weight) * comp["pz"] - float(intz_weight) * comp["intz"]
    regime = compute_regime_features(
        out,
        comp,
        price_source=price_source,
        autocorr_window=autocorr_window,
        vol_window=vol_window,
    )
    out["signal"] = compute_signals_from_components(
        out,
        comp,
        sigma,
        signal_mode=signal_mode,
        confirm_intervals=confirm_intervals,
        trend_ema_span=trend_ema_span,
        trend_filter_mode=trend_filter_mode,
        direction=direction,
        pz_weight=pz_weight,
        intz_weight=intz_weight,
        trend_prices=out["vwap_last"].to_numpy(dtype=np.float64)
        if price_source == "vwap"
        else out["close_last"].to_numpy(dtype=np.float64),
        trend_strength_min=trend_strength_min,
        autocorr_window=autocorr_window,
        autocorr_mode=autocorr_mode,
        autocorr_threshold=autocorr_threshold,
        autocorr=regime.get("autocorr"),
        vol_window=vol_window,
        vol_ratio_min=vol_ratio_min,
        vol_ratio_max=vol_ratio_max,
        vol_ratio=regime.get("vol_ratio"),
    )
    return out
