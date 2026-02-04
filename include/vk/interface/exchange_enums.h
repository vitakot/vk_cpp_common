/**
Common Exchange Enums

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_EXCHANGE_ENUMS_H
#define INCLUDE_VK_INTERFACE_EXCHANGE_ENUMS_H

#include <cstdint>

namespace vk {
enum class ExchangeId : std::int32_t {
    BinanceFutures,
    BinanceSpot,
    BybitFutures,
    BybitSpot,
    MEXCFutures,
    MEXCSpot,
    OKXFutures,
    OKXSpot
};

enum class Side : std::int32_t {
    Sell = 0,
    Buy = 1
};

enum class OrderType : std::int32_t {
    Market = 0,
    Limit = 1,
    Stop = 2,
    StopLimit = 3
};

enum class OrderStatus : std::int32_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    PendingCancel = 4,
    Rejected = 5,
    Expired = 6
};

enum class TimeInForce : std::int32_t {
    GTC = 0,
    IOC = 1,
    FOK = 2
};

enum class MarketCategory : std::int32_t {
    Spot,
    Futures
};

/**
 * Candle Interval, values are in seconds
 */
enum class CandleInterval : std::int32_t {
    _1m = 60,
    _3m = 180,
    _5m = 300,
    _15m = 900,
    _30m = 1800,
    _1h = 3600,
    _2h = 7200,
    _4h = 14400,
    _6h = 21600,
    _8h = 28800,
    _12h = 43200,
    _1d = 86400,
    _3d = 259200,
    _1w = 604800,
    _1M = 2592000,
};
}

#endif //INCLUDE_VK_INTERFACE_EXCHANGE_ENUMS_H
