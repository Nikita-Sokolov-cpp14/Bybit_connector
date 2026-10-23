# Data loader for aggregated_1s.csv
from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd

REQUIRED_COLUMNS = [
    "ts",
    "open",
    "high",
    "low",
    "close",
    "buy_vol",
    "sell_vol",
    "trade_count",
    "vwap",
]


def _detect_sep(path: Path) -> str:
    with path.open("r", encoding="utf-8", errors="replace") as fh:
        first = fh.readline()
    if "\t" in first:
        return "\t"
    if ";" in first and "," not in first:
        return ";"
    return ","


def load_aggregated_1s(csv_path: str | Path) -> pd.DataFrame:
    """
    Load Bybit aggregated 1-second data.

    Expected columns:
    - ts: int64 (UTC ms start of second)
    - open, high, low, close: float
    - buy_vol, sell_vol: float (base asset)
    - trade_count: int/float
    - vwap: float

    The file is sorted by `ts` and duplicate timestamps are removed (last wins).
    """
    path = Path(csv_path)
    if not path.exists():
        raise FileNotFoundError(path)

    sep = _detect_sep(path)
    dtypes = {
        "ts": "int64",
        "open": "float64",
        "high": "float64",
        "low": "float64",
        "close": "float64",
        "buy_vol": "float64",
        "sell_vol": "float64",
        "trade_count": "float64",
        "vwap": "float64",
    }
    df = pd.read_csv(path, sep=sep, dtype=dtypes)
    df.columns = [str(c).strip() for c in df.columns]

    missing = set(REQUIRED_COLUMNS) - set(df.columns)
    if missing:
        raise ValueError(f"Missing columns in CSV: {sorted(missing)}")

    df = df[REQUIRED_COLUMNS]

    # Keep the data numeric even if a malformed row slipped through.
    for c in REQUIRED_COLUMNS[1:]:
        df[c] = pd.to_numeric(df[c], errors="coerce")

    df = df.dropna(subset=REQUIRED_COLUMNS)
    df["ts"] = df["ts"].astype("int64")
    df = df.sort_values("ts", kind="mergesort")
    df = df.drop_duplicates(subset=["ts"], keep="last").reset_index(drop=True)

    return df


def df_to_arrays(df: pd.DataFrame) -> dict[str, np.ndarray]:
    """Extract the 1s arrays used by the backtester (avoids repeated .values)."""
    return {
        "ts": df["ts"].to_numpy(dtype=np.int64, copy=False),
        "open": df["open"].to_numpy(dtype=np.float64, copy=False),
        "high": df["high"].to_numpy(dtype=np.float64, copy=False),
        "low": df["low"].to_numpy(dtype=np.float64, copy=False),
        "close": df["close"].to_numpy(dtype=np.float64, copy=False),
        "vwap": df["vwap"].to_numpy(dtype=np.float64, copy=False),
        "buy_vol": df["buy_vol"].to_numpy(dtype=np.float64, copy=False),
        "sell_vol": df["sell_vol"].to_numpy(dtype=np.float64, copy=False),
    }
