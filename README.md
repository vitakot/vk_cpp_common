# stonky-cpp-common

Shared C++ interfaces, data types and utilities used across all Stonky source code.

## Requirements

- C++20 compiler
- CMake 4.0+
- spdlog
- nlohmann_json (required when using exchange types or JSON utilities)
- magic_enum (required when using enum serialization)
- Boost.DLL (required when using the module system)

## Contents

### Exchange Interfaces (`stonky/interface/`)

#### `IExchangeDownloader`
Base interface for all market data downloaders (Binance, Bybit, OKX, MEXC, Hyperliquid, …).

```cpp
#include "stonky/interface/i_exchange_downloader.h"

struct IExchangeDownloader {
    // Download OHLCV candles to CSV files
    virtual void updateMarketData(const std::string& dirPath,
                                  const std::vector<std::string>& symbols,
                                  CandleInterval candleInterval,
                                  const onSymbolsToUpdate& onSymbolsToUpdateCB,
                                  const onSymbolCompleted& onSymbolCompletedCB,
                                  bool convertToT6) const = 0;

    // Download funding rate history to CSV files
    virtual void updateFundingRateData(const std::string& dirPath,
                                       const std::vector<std::string>& symbols,
                                       const onSymbolsToUpdate& onSymbolsToUpdateCB,
                                       const onSymbolCompleted& onSymbolCompletedCB) const = 0;

    // Convert existing CSV files to Zorro T6 binary format
    virtual void convertToT6(const std::string& dirPath,
                              CandleInterval candleInterval) const = 0;
};
```

#### `IExchangeConnector`
Base interface for live trading connectors (order placement, account management).

```cpp
#include "stonky/interface/i_exchange_connector.h"

struct IExchangeConnector {
    virtual void login(const std::tuple<std::string, std::string, std::string>& credentials) = 0;
    virtual Trade placeOrder(const Order& order) = 0;
    virtual Balance getAccountBalance(const std::string& currency) const = 0;
    virtual std::vector<FundingRate> getFundingRates() const = 0;
    virtual std::vector<Candle> getHistoricalCandles(...) const = 0;
    // ...
};
```

Supported exchange IDs: `Demo`, `BinanceFutures`, `BinanceSpot`, `BybitFutures`, `BybitSpot`, `MEXCFutures`, `MEXCSpot`, `OKXFutures`, `OKXSpot`.

#### `IJson`
Mixin for types that support JSON serialisation / deserialisation.

```cpp
#include "stonky/interface/i_json.h"

struct IJson {
    virtual nlohmann::json toJson() const = 0;
    virtual void fromJson(const nlohmann::json& json) = 0;
};
```

---

### Common Data Types (`stonky/interface/exchange_types.h`)

| Type | Description |
|------|-------------|
| `Order` | Order request (symbol, side, type, quantity, price, …) |
| `Trade` | Order result (fill price, filled quantity, status, …) |
| `Candle` | OHLCV bar (openTime, open, high, low, close, volume) |
| `TickerPrice` | Best bid/ask, 24h volume and turnover |
| `Balance` | Account balance for a single currency |
| `FundingRate` | Funding rate snapshot (symbol, rate, timestamp) |
| `Symbol` | Instrument metadata (base/quote asset, contract size, …) |
| `Position` | Open position (symbol, side, size, avg price, leverage) |

---

### Enumerations (`stonky/interface/exchange_enums.h`)

```cpp
enum class Side        : int32_t { Sell, Buy };
enum class OrderType   : int32_t { Market, Limit, Stop, StopLimit };
enum class OrderStatus : int32_t { New, PartiallyFilled, Filled, Cancelled, PendingCancel, Rejected, Expired };
enum class TimeInForce : int32_t { GTC, IOC, FOK };
enum class MarketCategory : int32_t { Spot, Futures };

// Values are in seconds
enum class CandleInterval : int32_t {
    _1m=60, _3m=180, _5m=300, _15m=900, _30m=1800,
    _1h=3600, _2h=7200, _4h=14400, _6h=21600, _8h=28800, _12h=43200,
    _1d=86400, _3d=259200, _1w=604800, _1M=2592000
};
```

---

### Utilities (`stonky/utils/`)

#### `utils.h`
General-purpose helpers:

| Function / Type | Description |
|-----------------|-------------|
| `T6` struct | Zorro Trader binary bar format (time, OHLCV as floats) |
| `convertTimeMs(int64_t ms)` | Unix ms → Zorro `DATE` (OLE Automation double) |
| `convertTimeMs(DATE)` | Zorro `DATE` → Unix ms |
| `convertTimeS(int64_t s)` | Unix seconds → Zorro `DATE` |
| `splitString(s, delim)` | Split string by delimiter |
| `createDirectoryRecursively(path)` | Create nested directories, returns `std::error_code` |
| `formatDouble(precision, val)` | Format double with given decimal places |
| `getDateTimeStringFromTimeStamp(ts, fmt, isMs)` | Timestamp → formatted datetime string |
| `convertISOToMilliseconds(dateStr)` | ISO 8601 string → Unix ms |

#### `semaphore.h`
Counting semaphore for limiting parallelism, plus a `std::future` readiness helper:

```cpp
#include "stonky/utils/semaphore.h"

Semaphore sem(4);  // allow 4 concurrent slots

auto f = std::async(std::launch::async, [&sem]() {
    std::scoped_lock w(sem);  // blocks until a slot is free
    // ... work ...
});

if (isReady(f)) { /* future has a result */ }
```

#### Other utilities
| Header | Description |
|--------|-------------|
| `utils/id_generator.h` | Thread-safe unique ID generation |
| `utils/json_utils.h` | JSON helper functions |
| `utils/log_utils.h` | Logger callback type (`onLogMessage`) |
| `utils/registry.h` | Type-keyed registry (maps type → instance) |

---

### Module System (`stonky/common/`)

Plugin loading via Boost.DLL for dynamic exchange connector libraries.

| Header | Description |
|--------|-------------|
| `common/module_manager.h` | Loads and manages shared library plugins |
| `common/module_factory.h` | Creates connector instances from a loaded plugin |
| `interface/i_module_factory.h` | Plugin entry-point interface |

---

### Bundled Third-Party Headers

| Header | Library | Purpose |
|--------|---------|---------|
| `csv.h` | [vincentlaucsb/csv-parser](https://github.com/vincentlaucsb/csv-parser) | Fast CSV reading |
| `date.h` | [HowardHinnant/date](https://github.com/HowardHinnant/date) | Calendar / timezone utilities |
| `base64.h` | — | Base64 encode / decode |

---

## Project Structure

```
stonky-cpp-common/
├── include/
│   ├── stonky/
│   │   ├── interface/
│   │   │   ├── exchange_enums.h          # Common enumerations
│   │   │   ├── exchange_types.h          # Common data structures
│   │   │   ├── i_exchange_connector.h    # Trading connector interface
│   │   │   ├── i_exchange_downloader.h   # Data downloader interface
│   │   │   ├── i_json.h                  # JSON serialisation mixin
│   │   │   ├── i_module_factory.h        # Plugin factory interface
│   │   │   └── i_trade_rw.h             # Trade persistence interface
│   │   ├── common/
│   │   │   ├── module_manager.h
│   │   │   └── module_factory.h
│   │   └── utils/
│   │       ├── utils.h                   # General utilities + T6 type
│   │       ├── semaphore.h               # Counting semaphore + isReady()
│   │       ├── id_generator.h
│   │       ├── json_utils.h
│   │       ├── log_utils.h
│   │       └── registry.h
│   ├── base64.h                          # Bundled: Base64
│   ├── csv.h                             # Bundled: CSV reader
│   └── date.h                            # Bundled: date library
└── src/
    └── utils.cpp
```

## License

MIT License — see source files for details.

## Author

Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
