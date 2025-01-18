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

enum class OrderSide : std::int32_t {
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

enum class CandleInterval : std::int32_t {
    _1m,
    _3m,
    _5m,
    _15m,
    _30m,
    _1h,
    _2h,
    _4h,
    _6h,
    _8h,
    _12h,
    _1d,
    _3d,
    _1w,
    _1M
};
}

#endif //INCLUDE_VK_INTERFACE_EXCHANGE_ENUMS_H
