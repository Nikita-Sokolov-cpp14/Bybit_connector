# PFD indicator calculation matching C++ logic
import numpy as np
import pandas as pd


def _interval_key(ts_ms: int, interval_sec: int) -> int:
    """Bucket start timestamp (ms) by interval_sec."""
    iv_ms = interval_sec * 1000
    return (ts_ms // iv_ms) * iv_ms


def aggregate_intervals(df_1s: pd.DataFrame, interval_sec: int) -> pd.DataFrame:
    """
    Aggregate 1s data into intervals of interval_sec.

    Returns DataFrame per interval with:
    - ts_interval: start ms of interval (bucket)
    - buy_vol_acc, sell_vol_acc: sums
    - flow: (buy-sell)/(buy+sell) if sum>1e-12 else 0.0
    - price_mid: mid price for interval (from last second's vwap/close depending on use)
    - close_last, vwap_last, high_max, low_min, close_interval (alias)
    - count_sec: number of seconds in interval
    """
    if df_1s.empty:
        return pd.DataFrame()

    df = df_1s.copy()
    df["ts_interval"] = df["ts"].apply(lambda x: _interval_key(x, interval_sec))

    g = df.groupby("ts_interval", sort=True)

    res = pd.DataFrame()
    res["buy_vol_acc"] = g["buy_vol"].sum().values
    res["sell_vol_acc"] = g["sell_vol"].sum().values
    res["high_max"] = g["high"].max().values
    res["low_min"] = g["low"].min().values
    res["close_interval"] = g["close"].last().values
    res["close_last"] = g["close"].last().values
    res["vwap_last"] = g["vwap"].last().values
    res["open_first"] = g["open"].first().values
    res["count_sec"] = g["ts"].count().values
    res["ts_interval"] = g["ts_interval"].first().values

    buy = res["buy_vol_acc"].values
    sell = res["sell_vol_acc"].values
    ssum = buy + sell
    flow = np.where(ssum > 1e-12, (buy - sell) / ssum, 0.0)
    res["flow"] = flow

    # price_mid placeholder (set by caller by price_source)
    res["price_mid"] = res["vwap_last"].values

    res = res.sort_values("ts_interval").reset_index(drop=True)
    return res


def compute_pfd_signals(df_intervals: pd.DataFrame, window_size: int, sigma: float, price_source: str = "vwap") -> pd.DataFrame:
    """
    Compute PFD signals per interval (event when new interval arrives).
    Logic matches C++:
    - price_mid = vwap_last or close_last
    - price_series and flow_series length W+1. Signal only when full (len == W+1)
    - tr = price_series[-1]/price_series[0] - 1
    - rets i from 1..W: (p_i/p_{i-1}-1), mean, sigPx = sqrt(sum((r-mean)^2/(W-1)))*sqrt(W)
    - flow_window = flow_series[-W:] (last W), mean/stdFlow with W-1
    - pz = tr/sigPx, intz = meanFlow*sqrt(W)/stdFlow, comb = pz-intz
    - guards sigPx<1e-9 or stdFlow<1e-9 -> comb=0
    - +1 if comb>=sigma, -1 if comb<=-sigma else 0
    """
    if df_intervals.empty:
        out = df_intervals.copy()
        out["signal"] = 0
        return out

    W = int(window_size)
    out = df_intervals.copy()

    # price_mid
    if price_source == "close":
        out["price_mid"] = out["close_last"].values
    else:
        out["price_mid"] = out["vwap_last"].values

    n = len(out)
    signals = np.zeros(n, dtype=int)

    if n < (W + 1):
        out["signal"] = signals
        return out

    price_mid_arr = out["price_mid"].values.astype(float)
    flow_arr = out["flow"].values.astype(float)

    # Build rolling buffers over full history; check at each new index when filled
    for i in range(n):
        # need W+1 points ending at i? buffers are [i - W, ..., i] total W+1
        start = i - W
        if start < 0:
            continue  # not filled yet
        # filled: indices start..i inclusive count = W+1
        price_series = price_mid_arr[start : i + 1]  # length W+1
        flow_series = flow_arr[start : i + 1]  # length W+1

        # tr: momentum over W intervals
        p0 = price_series[0]
        p_last = price_series[-1]
        if p0 <= 0.0:
            tr = 0.0
        else:
            tr = p_last / p0 - 1.0

        # price returns rets[1..W]
        sigPx = 0.0
        rets = price_series[1:] / price_series[:-1] - 1.0  # length W
        # mean and std over W rets, ddof=1 (W-1)
        meanRet = np.mean(rets)
        varRet = np.var(rets, ddof=1) if W > 1 else 0.0
        if W <= 1:
            sigPx = 0.0
        else:
            sigPx = np.sqrt(varRet) * np.sqrt(W)

        # flow window last W
        flow_window = flow_series[-W:] if W >= 0 else np.array([])
        meanFlow = float(np.mean(flow_window)) if flow_window.size > 0 else 0.0
        stdFlow = 0.0
        if W > 1 and flow_window.size >= 2:
            stdFlow = float(np.sqrt(np.var(flow_window, ddof=1)))
        elif W == 1:
            stdFlow = 0.0

        # comb
        comb = 0.0
        if sigPx >= 1e-9 and stdFlow >= 1e-9:
            pz = tr / sigPx
            intz = meanFlow * np.sqrt(W) / stdFlow
            comb = pz - intz
        else:
            comb = 0.0

        # signal for interval i
        if comb >= sigma:
            signals[i] = 1
        elif comb <= -sigma:
            signals[i] = -1
        else:
            signals[i] = 0

    out["signal"] = signals
    return out
