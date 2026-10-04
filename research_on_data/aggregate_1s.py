#!/usr/bin/env python3
"""
Агрегация потока публичных сделок Bybit (publicTrade) в 1-секундные бары.

Читает tab-separated лог log_files/public_trade_data.csv (записывается в
ConnectionManager::logPublicTradeData) и сохраняет агрегаты в
research_on_data/aggregated_1s/aggregated_1s.csv.

Исходные столбцы: ts, mid_price, T, v, p, L, BT, RPI, S, s, seq
Используются:    ts (мс UTC), v (объем), p (цена), L (tick direction), S (сторона)

Столбцы результата:
    ts          начало секунды, мс UTC
    open/high/low/close  цены
    buy_vol/sell_vol     объем, разбитый по стороне сделки
    trade_count          число сделок
    vwap                 средневзвешенная цена (по всем сделкам секунды)

Запуск:
    python3 aggregate_1s.py
    python3 aggregate_1s.py --input ../log_files/public_trade_data.csv \
                            --output aggregated_1s/aggregated_1s.csv
"""

import argparse
import io
import os
import sys
import time

import numpy as np
import pandas as pd

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_INPUT = os.path.join(SCRIPT_DIR, os.pardir, "log_files", "public_trade_data.csv")
DEFAULT_OUTPUT = os.path.join(SCRIPT_DIR, "aggregated_1s", "aggregated_1s.csv")

OUTPUT_COLUMNS = [
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

USECOLS = ["ts", "v", "p", "L", "S"]
DTYPES = {"ts": "float64", "v": "float64", "p": "float64", "L": "float64", "S": "object"}

# Тик-направление (L): 1, 3 -> покупка, 2, 4 -> продажа.
TICK_BUY = (1.0, 3.0)
TICK_SELL = (2.0, 4.0)

CHUNK_SIZE = 2_000_000
FLUSH_LINES = 200_000
SCAN_BLOCK = 4 << 20


def fmt(value: float) -> str:
    """Число без хвостовых нулей: 0.03 -> '0.03', 0.30000000000000004 -> '0.3'."""
    text = f"{value:.10f}".rstrip("0").rstrip(".")
    if text in ("", "-", "-0"):
        return "0"
    return text


def aggregate_chunk(chunk: pd.DataFrame, row_offset: int) -> tuple:
    """Агрегаты чанка по секундам.

    Возвращает (frame, unknown_side), где frame индексирован секундой.
    Внутри frame: open/high/low/close/buy_vol/sell_vol/vol/pv/cnt/ord_first/ord_last,
    ord_* — глобальный номер строки, по нему определяются open/close секунды.
    """
    ts = chunk["ts"].to_numpy()
    price = chunk["p"].to_numpy()
    volume = chunk["v"].to_numpy()
    tick = chunk["L"].to_numpy()

    # Основной случай — S равно "Buy"/"Sell"; копий строк не создаём.
    raw_side = chunk["S"].to_numpy()
    is_buy = raw_side == "Buy"
    is_sell = raw_side == "Sell"
    unresolved = ~(is_buy | is_sell)
    if unresolved.any():
        # Регистр/пробелы/пустые значения — нормализуем только такие строки.
        rest = pd.Series(raw_side[unresolved]).fillna("").astype(str).str.strip().str.lower()
        is_buy[unresolved] = rest.eq("buy").to_numpy()
        is_sell[unresolved] = rest.eq("sell").to_numpy()

    # Если поле S пустое/неизвестное, определяем сторону по L.
    buy = np.where(is_buy, True, np.where(is_sell, False, np.isin(tick, TICK_BUY)))
    sell = np.where(is_sell, True, np.where(is_buy, False, np.isin(tick, TICK_SELL)))
    unknown_side = int(np.count_nonzero(~(buy | sell)))

    trades = pd.DataFrame(
        {
            "bucket": (ts // 1000).astype("int64"),
            "price": price,
            "buy_vol": np.where(buy, volume, 0.0),
            "sell_vol": np.where(sell, volume, 0.0),
            "vol": volume,
            "pv": price * volume,
            "cnt": 1,
            "ord": np.arange(row_offset, row_offset + len(chunk), dtype="int64"),
        }
    )

    grouped = trades.groupby("bucket", sort=False)
    part = grouped.agg(
        high=("price", "max"),
        low=("price", "min"),
        buy_vol=("buy_vol", "sum"),
        sell_vol=("sell_vol", "sum"),
        vol=("vol", "sum"),
        pv=("pv", "sum"),
        cnt=("cnt", "sum"),
        ord_first=("ord", "min"),
        ord_last=("ord", "max"),
    )
    part["open"] = grouped["price"].first()
    part["close"] = grouped["price"].last()
    return part, unknown_side


def data_limit(input_path: str) -> int:
    """Размер читаемой части файла: всё до первого нулевого байта.

    Лог дописывается приложением, и после обрыва записи его хвост бывает забит
    нулями. Такие байты pandas читает как одну гигантскую строку, поэтому их
    отбрасываем.
    """
    limit = os.path.getsize(input_path)
    with open(input_path, "rb") as source:
        offset = 0
        while True:
            block = source.read(SCAN_BLOCK)
            if not block:
                break
            zero = block.find(b"\x00")
            if zero >= 0:
                limit = offset + zero
                break
            offset += len(block)
    return limit


class LimitedFile(io.RawIOBase):
    """Потоковое чтение первых limit байт файла."""

    def __init__(self, path: str, limit: int):
        self._source = open(path, "rb")
        self._limit = limit

    def readable(self) -> bool:
        return True

    def readinto(self, buffer) -> int:
        remaining = self._limit - self._source.tell()
        if remaining <= 0:
            return 0
        data = self._source.read(min(len(buffer), remaining))
        buffer[: len(data)] = data
        return len(data)

    def close(self) -> None:
        self._source.close()
        super().close()


def open_input(input_path: str):
    """Открывает лог, при необходимости обрезая нулевой хвост."""
    limit = data_limit(input_path)
    size = os.path.getsize(input_path)
    if limit < size:
        print(
            f"Внимание: нулевой хвост {size - limit:,} байт "
            f"({(size - limit) / 2**20:.1f} МБ) — игнорируется",
            file=sys.stderr,
        )
        return io.BufferedReader(LimitedFile(input_path, limit))
    return open(input_path, "rb")


def aggregate_file(input_path: str, chunk_size: int) -> tuple:
    """Читает файл порциями и возвращает (frame со сводными агрегатами, статистика)."""
    parts = []
    stats = {"rows": 0, "skipped": 0, "unknown_side": 0}
    started = time.perf_counter()

    with open_input(input_path) as source:
        reader = pd.read_csv(
            source,
            sep="\t",
            usecols=USECOLS,
            dtype=DTYPES,
            chunksize=chunk_size,
            on_bad_lines="skip",
        )

        for index, chunk in enumerate(reader):
            stats["rows"] += len(chunk)
            # Отбрасываем битые строки (в логе встречается хвост из нулей при обрыве записи).
            valid = chunk["ts"].notna() & chunk["p"].notna() & chunk["v"].notna()
            valid &= (chunk["ts"] > 0) & (chunk["p"] > 0) & (chunk["v"] >= 0)
            bad = int((~valid).sum())
            if bad:
                stats["skipped"] += bad
                chunk = chunk[valid]
            if chunk.empty:
                continue

            part, unknown_side = aggregate_chunk(chunk, stats["rows"] - len(chunk))
            parts.append(part)
            stats["unknown_side"] += unknown_side
            del chunk  # освобождаем чанк до чтения следующего, иначе пик памяти x2

            elapsed = time.perf_counter() - started
            print(
                f"[{index + 1}] строк прочитано: {stats['rows']:,} "
                f"(пропущено: {stats['skipped']:,}), секунд: {len(part):,} "
                f"| {elapsed:.1f} c",
                file=sys.stderr,
                flush=True,
            )

    if not parts:
        raise SystemExit("Не удалось прочитать ни одной корректной сделки.")

    all_parts = pd.concat(parts)
    total = all_parts.groupby("bucket").agg(
        high=("high", "max"),
        low=("low", "min"),
        buy_vol=("buy_vol", "sum"),
        sell_vol=("sell_vol", "sum"),
        vol=("vol", "sum"),
        pv=("pv", "sum"),
        cnt=("cnt", "sum"),
    )
    # open/close берём по первой/последней строке исходного лога внутри секунды.
    total["open"] = all_parts.sort_values("ord_first").groupby("bucket")["open"].first()
    total["close"] = all_parts.sort_values("ord_last").groupby("bucket")["close"].last()
    total["vwap"] = np.divide(
        total["pv"], total["vol"], out=np.zeros(len(total)), where=total["vol"] > 0
    )
    return total, stats


def write_aggregated(total: pd.DataFrame, output_path: str) -> None:
    output_dir = os.path.dirname(os.path.abspath(output_path))
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    rows = zip(
        (total.index.to_numpy() * 1000).tolist(),
        total["open"].tolist(),
        total["high"].tolist(),
        total["low"].tolist(),
        total["close"].tolist(),
        total["buy_vol"].tolist(),
        total["sell_vol"].tolist(),
        total["cnt"].tolist(),
        total["vwap"].tolist(),
    )

    buffer = []
    with open(output_path, "w", newline="") as out:
        out.write("\t".join(OUTPUT_COLUMNS) + "\n")
        for row in rows:
            buffer.append(
                "\t".join(fmt(value) if index else str(value) for index, value in enumerate(row))
            )
            if len(buffer) >= FLUSH_LINES:
                out.write("\n".join(buffer) + "\n")
                buffer.clear()
        if buffer:
            out.write("\n".join(buffer) + "\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Агрегация публичных сделок в 1-секундные бары")
    parser.add_argument("--input", default=DEFAULT_INPUT, help="путь к логу public_trade_data.csv")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="путь к итоговому csv")
    parser.add_argument(
        "--chunk-size", type=int, default=CHUNK_SIZE, help="строк в порции чтения"
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    started = time.perf_counter()

    print(f"Чтение: {args.input}", file=sys.stderr)
    total, stats = aggregate_file(os.path.abspath(args.input), args.chunk_size)

    total.sort_index(inplace=True)
    write_aggregated(total, os.path.abspath(args.output))

    elapsed = time.perf_counter() - started
    print(
        f"\nСделок учтено: {int(total['cnt'].sum()):,}\n"
        f"Строк в файле: {stats['rows']:,} (пропущено битых: {stats['skipped']:,}, "
        f"без определённой стороны: {stats['unknown_side']:,})\n"
        f"Секунд в результате: {len(total):,}\n"
        f"Диапазон: {total.index[0] * 1000} - {total.index[-1] * 1000} (мс UTC)\n"
        f"Записано в: {os.path.abspath(args.output)}\n"
        f"Время: {elapsed:.1f} с",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
