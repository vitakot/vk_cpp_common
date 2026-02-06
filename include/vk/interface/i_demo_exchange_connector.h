/**
Demo Exchange Connector Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2026 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_I_DEMO_EXCHANGE_CONNECTOR_H
#define INCLUDE_VK_INTERFACE_I_DEMO_EXCHANGE_CONNECTOR_H

#include <vk/interface/i_exchange_connector.h>

namespace vk {

/**
 * Configuration for exchange connector
 */
struct ExchangeConfig {
   std::filesystem::path fundingRatesDir; /**< Directory with funding rate CSV files */
   std::filesystem::path priceDataDir;    /**< Directory with OHLCV price CSV files */
   double initialEquity = 10000.0;        /**< Initial account equity in quote currency */
   double takerFee = 0.00055;             /**< Taker fee rate (default 0.055%) */
   double makerFee = 0.0002;              /**< Maker fee rate (default 0.02%) */
   std::int64_t startTime = 0;            /**< Backtest start time in ms (0 = use the earliest data) */
   std::int64_t endTime = 0;              /**< Backtest end time in ms (0 = use latest data) */
};

struct BOOST_SYMBOL_VISIBLE IDemoExchangeConnector : IExchangeConnector {
   ~IDemoExchangeConnector() override = default;

   /**
    * Initialize backtest with configuration
    * @param config Exchange configuration
    * @throws runtime_error
    */
   virtual void initialize(const ExchangeConfig& config) = 0;

   /**
    * Advance simulation time by specified duration
    * @param durationMs Duration in milliseconds
    * @return New simulation time in ms, or 0 if end of data reached
    */
   virtual std::int64_t advanceTimeBy(std::int64_t durationMs) = 0;

   /**
    * Set simulation time directly
    * @param timeMs Time in milliseconds since epoch
    */
   virtual void setTime(std::int64_t timeMs) = 0;

   /**
    * Get current equity (initial + realized PnL - commissions)
    * @return Current equity in quote currency
    */
   [[nodiscard]] virtual double getEquity() const = 0;

   /**
    * Get total realized PnL
    * @return Realized profit/loss in quote currency
    */
   [[nodiscard]] virtual double getRealizedPnL() const = 0;

   /**
    * Get total commissions paid
    * @return Total commissions in quote currency
    */
   [[nodiscard]] virtual double getTotalCommissions() const = 0;

   /**
    * Check if backtest has reached end of data
    * @return True if no more data available
    */
   [[nodiscard]] virtual bool isFinished() const = 0;

   /**
    * Reset backtest to initial state
    */
   virtual void reset() = 0;
};
}  // namespace vk
#endif  // INCLUDE_VK_INTERFACE_I_DEMO_EXCHANGE_CONNECTOR_H
