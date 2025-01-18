/**
Common Exchange Types

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_EXCHANGE_TYPES_H
#define INCLUDE_VK_INTERFACE_EXCHANGE_TYPES_H

#include "exchange_enums.h"
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace vk {
struct Order {
    /// Order quantity
    double quantity{};

    /// Symbol name - e.g. BTCUSDT
    std::string symbol{};

    /// Order side - e.g. OrderSide::Buy
    OrderSide side{OrderSide::Buy};

    /// Order type - e.g. OrderType::Limit
    OrderType type{OrderType::Limit};

    /// Time in force - e.g. TimeInForce::GTC
    TimeInForce timeInForce{TimeInForce::GTC};

    /// Either limit or stop price
    double price{};

    /// Client order id
    std::string clientOrderId{};

    std::unique_ptr<nlohmann::json> customData{};
};

struct Trade {
    /// Average realized price
    double averagePrice{};

    /// Realized quantity
    double filledQuantity{};

    /// Fill timestamp in ms since Epoch
    std::int64_t fillTime{4};

    /// Order status - e.g. OrderStatus::New
    OrderStatus orderStatus{OrderStatus::New};

    std::unique_ptr<nlohmann::json> customData{};
};

struct TickerPrice {
    double bidPrice{};
    double askPrice{};
    double bidQty{};
    double askQty{};
    std::int64_t time{};

    std::unique_ptr<nlohmann::json> customData{};
};

struct Balance {
    double balance{};

    std::unique_ptr<nlohmann::json> customData{};
};

struct FundingRate {
    std::string m_symbol{};
    double m_fundingRate{};
    std::int64_t m_fundingTime{};

    std::unique_ptr<nlohmann::json> customData{};
};
}

#endif //INCLUDE_VK_INTERFACE_EXCHANGE_TYPES_H
