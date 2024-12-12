/**
Exchange Connector Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_TOOLS_I_EXCHANGE_CONNECTOR_H
#define INCLUDE_VK_TOOLS_I_EXCHANGE_CONNECTOR_H

#include <string>
#include <boost/config.hpp>
#include <boost/dll/alias.hpp>
#include <vk/tools/log_utils.h>
#include <enum.h>

BETTER_ENUM(ExchangeId, std::int32_t,
            BinanceFutures,
            Bybit,
            MEXC,
            OKX
)

enum OrderSide {
    Sell = 0,
    Buy = 1
};

enum OrderType {
    Market = 0,
    Limit = 1,
    Stop = 2,
    StopLimit = 3
};

enum OrderStatus {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    PendingCancel = 4,
    Rejected = 5,
    Expired = 6
};

enum TimeInForce {
    GTC = 0,
    IOC = 1,
    FOK = 2
};

struct Order {
    /// Order quantity
    double quantity = 0.0;

    /// Symbol name - e.g. BTCUSDT
    std::string asset;

    /// Order side - e.g. OrderSide::Buy
    OrderSide side = Buy;

    /// Order type - e.g. OrderType::Limit
    OrderType type = Limit;

    /// Time in force - e.g. TimeInForce::GTC
    TimeInForce timeInForce = GTC;

    /// Either limit or stop price
    double price = 0.0;

    /// Client order id
    std::string clientOrderId;
};

struct Trade {
    /// Average realized price
    double averagePrice = 0.0;

    /// Realized quantity
    double filledQuantity = 0.0;

    /// Fill timestamp in ms since Epoch
    std::int64_t fillTime = 4;

    /// Order status - e.g. OrderStatus::New
    OrderStatus orderStatus = New;
};

struct Price {
    double bidPrice = 0.0;

    double askPrice = 0.0;
};

struct Balance {
    double balance = 0.0;
};

struct BOOST_SYMBOL_VISIBLE IExchangeConnector {
    virtual ~IExchangeConnector() = default;

    [[nodiscard]] virtual std::string version() const = 0;

    [[nodiscard]] virtual std::string name() const = 0;

    virtual void setLoggerCallback(const onLogMessage& onLogMessageCB) = 0;

    virtual void login(const std::tuple<std::string, std::string, std::string>& credentials) = 0;

    virtual void setDemo(bool demo) = 0;

    [[nodiscard]] virtual bool isDemo() const = 0;

    virtual Trade trade(const Order& order) = 0;

    [[nodiscard]] virtual Price getAssetPrice(const std::string& asset) const = 0;

    [[nodiscard]] virtual Balance getAccountBalance(const std::string& currency) const = 0;
};

#endif // INCLUDE_VK_TOOLS_I_EXCHANGE_CONNECTOR_H
