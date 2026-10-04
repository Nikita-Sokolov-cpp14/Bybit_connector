# Utils
import numpy as np
import pandas as pd


def attach_close_last_ts(df_intervals: pd.DataFrame, df_1s: pd.DataFrame, interval_sec: int) -> pd.DataFrame:
    """Attach close_last_ts (ms of last second in interval) for easier backtest."""
    if df_intervals.empty:
        return df_intervals.copy()
    out = df_intervals.copy()
    # map by ts_interval
    # build map
    last_ts_map = {}
    if not df_1s.empty:
        d = df_1s.copy()
        d["ts_interval"] = (d["ts"] // (interval_sec*1000)) * (interval_sec*1000)
        g = d.groupby("ts_interval")["ts"].last()
        last_ts_map = g.to_dict()
    out["close_last_ts"] = out["ts_interval"].map(last_ts_map).fillna(out["ts_interval"] + interval_sec*1000 - 1000).astype("int64")
    return out
