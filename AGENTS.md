# HFT — Bybit high-frequency trading bot

## Build & run

```bash
# Linux (native)
mkdir build && cd build && cmake .. && make -j$(nproc)
./connector            # run from build/ (configs loaded as ../resources/*.ini)

# Windows cross-compile (MinGW)
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw64.cmake -DCMAKE_CROSSCOMPILING=ON .. && make
```

No test/lint/typecheck scripts exist. Format manually with `clang-format -i <file>` (config at `.clang-format`). C++23, CMake ≥3.5. Dependencies: **Boost** (locale, beast, asio, property_tree, circular_buffer), **OpenSSL**.

## Architecture

Single CMake project producing one executable (`connector`) from static libraries:

| Directory | CMake target | Role |
|---|---|---|
| `websocket_client/` | `websocket_client` | WebSocket I/O (public data, private data, order sending) |
| `data_structures/` | `data_structures` | Core types: OrderBook, Trade, Order, Position, Wallet, etc. |
| `json_parser/` | `json_parser` | JSON (de)serialization per data structure |
| `connection_manager/` | `connection_manager` | Owns 3 io_contexts (public/private/orders), wires websockets → strategy |
| `trading_strategy/` | `trading_strategy` | Strategy base + `ImbalanceAndLarge` (OBI+TFI indicators) |
| `trading_strategy/indicators/` | `indicators` | OrderBookImbalance, TradeFlowImbalance |
| `trading_strategy/trade_manager/` | `trade_manager` | Order lifecycle management |
| `p999_latency/` | `check_latency` | P999 latency measurement |
| `check_parsing/` | `check_parsing` | Parsing verification |
| `check_place_orders/` | `check_place_orders` | Order placement test helpers |
| `utils/` | `utils` | INI config reader (`readAuthConfig`, `readConnectionConfig`) |
| `loger/` | `loger` | CSV logger |
| `limits/` | — | Config file path constants (header-only) |

Entrypoint: `main.cpp` creates `ImbalanceAndLarge` + `ConnectionManager`, subscribes, calls `connect()`.

## Config

INI files in `resources/` — paths hardcoded in `limits/limits.h` as `../resources/{auth,connection}.ini` (relative to CWD). `auth.ini` contains API key/secret (real + demo commented). `connection.ini` points at `stream.bybit.com` (v5 public/private/trade endpoints).

## Key quirks

- `limits/limits.h` is **header-only** — include it directly (no library target).
- `data_structures/common_data.h` defines foundational enums (`Side`, `Category`, `OrderType`).
- `trading_strategy/settings.h` contains all tunable parameters (depths, sigmas, timeouts, leverage=50).
- Config paths **must** resolve relative to the working directory at runtime — run from `build/` or adjust.
- `experiment/` is a **separate standalone CMake project** (same name `connector`, own `CMakeLists.txt` + `main.cpp`).
- `log_files/` holds CSV output (`total_signals.csv`, `imbalance.csv`, `trade_flow.csv`) and dated subdirectories. Gitignored.
- `decoder/` contains Python scripts (`decoder_full.py`, etc.) for offline signal analysis — not part of the C++ build.
- Cross-compile uses `toolchain-mingw64.cmake` with hardcoded paths to `win_libraries/` (Boost + OpenSSL .a files).
- No snapshot/CI/github workflow config present.
