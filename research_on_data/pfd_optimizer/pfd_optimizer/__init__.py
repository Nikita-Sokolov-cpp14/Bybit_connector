from dataclasses import dataclass
from typing import Optional

@dataclass
class PFDConfig:
    interval_sec: int = 60
    window_size: int = 360
    sigma: float = 2.0
    price_source: str = "vwap"  # "vwap" or "close"
    take_profit: float = 0.01
    stop_loss: float = 0.005
    max_hold_sec: int = 3600
    fee_bps: float = 16.0  # 0.16%
    conservative_intrabar: bool = True
