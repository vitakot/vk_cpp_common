/**
Exchange Connector Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_I_EXCHANGE_CONNECTOR_H
#define INCLUDE_VK_INTERFACE_I_EXCHANGE_CONNECTOR_H

#include "exchange_types.h"
#include <string>
#include <boost/dll/alias.hpp>
#include <vk/utils/log_utils.h>

namespace vk {
struct BOOST_SYMBOL_VISIBLE IExchangeConnector {
    virtual ~IExchangeConnector() = default;

    [[nodiscard]] virtual std::string version() const = 0;

    [[nodiscard]] virtual std::string name() const = 0;

    virtual void setLoggerCallback(const onLogMessage& onLogMessageCB) = 0;

    virtual void login(const std::tuple<std::string, std::string, std::string>& credentials) = 0;

    virtual Trade placeOrder(const Order& order) = 0;

    [[nodiscard]] virtual TickerPrice getTickerPrice(const std::string& symbol) const = 0;

    [[nodiscard]] virtual Balance getAccountBalance(const std::string& currency) const = 0;

    [[nodiscard]] virtual FundingRate getLastFundingRate(const std::string& symbol) const = 0;

    [[nodiscard]] virtual std::vector<FundingRate> getFundingRates(const std::string& symbol, std::int64_t startTime,
                                                                   std::int64_t endTime) const = 0;

    [[nodiscard]] virtual std::vector<Ticker> getTickerInfo(const std::string& symbol) const = 0;

    [[nodiscard]] virtual std::int64_t getServerTime() const = 0;
};
}
#endif // INCLUDE_VK_INTERFACE_I_EXCHANGE_CONNECTOR_H
