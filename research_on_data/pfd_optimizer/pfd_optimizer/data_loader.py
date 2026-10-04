# Data loader for aggregated_1s.csv
import pandas as pd
import numpy as np
from pathlib import Path


def load_aggregated_1s(csv_path: str | Path) -> pd.DataFrame:
    """
    Load Bybit aggregated 1-second data.

    Expected columns:
    - ts: int64 (UTC ms start of second)
    - open, high, low, close: float
    - buy_vol, sell_vol: float (base asset)
    - trade_count: int/float
    - vwap: float
    """
    path = Path(csv_path)
    df = pd.read_csv(path)

    # Normalize column names if any spaces
    df.columns = [c.strip() for c in df.columns]

    required = {"ts", "open", "high", "low", "close", "buy_vol", "sell_vol", "trade_count", "vwap"}
    missing = required - set(df.columns)
    if missing:
        raise ValueError(f"Missing columns in CSV: {missing}")

    # Types
    df["ts"] = df["ts"].astype("int64")
    for c in ["open", "high", "low", "close", "buy_vol", "sell_vol", "vwap"]:
        df[c] = pd.to_numeric(df[c], errors="coerce")
    df["trade_count"] = pd.to_numeric(df["trade_count"], errors="coerce").astype("Int64")

    # Sort by time
    df = df.sort_values("ts").reset_index(drop=True)

    # Drop NaNs in core fields if any
    df = df.dropna(subset=["ts", "open", "high", "low", "close", "vwap", "buy_vol", "sell_vol"])

    return df
