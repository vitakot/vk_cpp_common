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
#include <nlohmann/json_fwd.hpp>
#include <vk/tools/log_utils.h>
#include <magic_enum.hpp>

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

struct BOOST_SYMBOL_VISIBLE IExchangeConnector {
    virtual ~IExchangeConnector() = default;

    [[nodiscard]] virtual std::string version() const = 0;

    [[nodiscard]] virtual std::string name() const = 0;

    virtual void setLoggerCallback(const onLogMessage &onLogMessageCB) = 0;

    virtual void login(const std::tuple<std::string, std::string, std::string> &credentials) = 0;

    virtual void setDemo(bool demo) = 0;

    [[nodiscard]] virtual bool isDemo() const = 0;

    virtual Trade placeOrder(const Order &order) = 0;

    [[nodiscard]] virtual TickerPrice getSymbolTickerPrice(const std::string &symbol) const = 0;

    [[nodiscard]] virtual Balance getAccountBalance(const std::string &currency) const = 0;

    [[nodiscard]] virtual FundingRate getLastFundingRate(const std::string &symbol) const = 0;

    [[nodiscard]] virtual std::vector<FundingRate> getFundingRates(const std::string &symbol, std::int64_t startTime,
                                                                   std::int64_t endTime) const = 0;
};

#endif // INCLUDE_VK_TOOLS_I_EXCHANGE_CONNECTOR_H
