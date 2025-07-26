/**
Exchange Connector Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_I_EXCHANGE_CONNECTOR_H
#define INCLUDE_VK_INTERFACE_I_EXCHANGE_CONNECTOR_H

#include <vk/utils/log_utils.h>
#include <vk/utils/semaphore.h>
#include <future>
#include "exchange_types.h"
#include <string>
#include <boost/dll/alias.hpp>

namespace vk {
struct BOOST_SYMBOL_VISIBLE IExchangeConnector {
    virtual ~IExchangeConnector() = default;

    /**
     * Returns version of Exchange connector
     * @return version string i.e. 1.0.0
     */
    [[nodiscard]] virtual std::string version() const = 0;

    /**
     * Returns Exchange ID i.e. name of the CEX, it must correspond to ExchangeId enum!
     * @return Exchange ID string
     */
    [[nodiscard]] virtual std::string exchangeId() const = 0;

    /**
     * Install logger callback
     * @param onLogMessageCB
     */
    virtual void setLoggerCallback(const onLogMessage& onLogMessageCB) = 0;

    /**
     * Login to exchange - for access to account related functions (order, balance...)
     * @param credentials - ApiKey, ApiSecret, PassPhrase (mostly optional)
     */
    virtual void login(const std::tuple<std::string, std::string, std::string>& credentials) = 0;

    /**
     * Place order on exchange, requires login using credentials
     * @param order
     * @return Trade structure with order result/state
     */
    virtual Trade placeOrder(const Order& order) = 0;

    /**
     * Get Account balance, requires login using credentials
     * @param currency
     * @return Balance structure
     */
    [[nodiscard]] virtual Balance getAccountBalance(const std::string& currency) const = 0;

    /**
     * Get funding rate for given symbol
     * @param symbol
     * @return FundingRate structure
     */
    [[nodiscard]] virtual FundingRate getFundingRate(const std::string& symbol) const = 0;

    /**
     * Get funding rates for all available symbols
     * @return vector of FundingRate structures
     */
    [[nodiscard]] virtual std::vector<FundingRate> getFundingRates() const = 0;

    /**
     * Get ticker price for give symbol
     * @param symbol
     * @return TickerPrice structure
     */
    [[nodiscard]] virtual TickerPrice getTickerPrice(const std::string& symbol) const = 0;

    /**
     * Get symbol info
     * @param symbol
     * @return vector of Ticker structures
     */
    [[nodiscard]] virtual std::vector<Ticker> getTickerInfo(const std::string& symbol) const = 0;

    /**
     * Get server Unix time in ms
     * @return timestamp in ms
     */
    [[nodiscard]] virtual std::int64_t getServerTime() const = 0;
};

template <typename R, typename T, typename... Args>
auto execute(const std::map<ExchangeId, std::shared_ptr<IExchangeConnector>>& exchanges, T method, Args&&... args) {
    std::vector<std::future<std::pair<ExchangeId, R>>> futures;
    std::map<ExchangeId, R> results;

    for (const auto& val : exchanges) {
        futures.push_back(std::async(std::launch::async, [val, method](Args&&... a) {
            std::pair<ExchangeId, R> retVal;
            retVal.first = val.first;
            retVal.second = (val.second.get()->*method)(std::forward<Args>(a)...);
            return retVal;
        }, std::forward<Args>(args)...));
    }

    do {
        for (auto& future : futures) {
            if (isReady(future)) {
                results.insert(future.get());
            }
        }
    }
    while (results.size() < futures.size());
    return results;
}
}
#endif // INCLUDE_VK_INTERFACE_I_EXCHANGE_CONNECTOR_H
