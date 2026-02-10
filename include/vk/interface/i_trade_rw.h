/**
Trade Reader/Writer Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_I_TRADE_RW_H
#define INCLUDE_VK_INTERFACE_I_TRADE_RW_H

#include <cstdint>
#include <optional>
#include <string>

namespace vk {

enum class FillOrder : std::int32_t { Market, Limit, StopMarket, StopLimit };

enum class FillState : std::int32_t { Filled, Cancelled, Open };

enum class FillOrderType : std::int32_t { TP, SL };

enum class AssetCategory : std::int32_t { Crypto, Forex, Stocks, Commodities };

struct Exchange {
   std::string name{};
   std::optional<std::string> description{};
   std::int64_t createdAt{};
};

struct TradeGroup {
   std::int32_t id{};
   std::string name{};
   bool isLive{true};
   std::int64_t startDate{};
   std::optional<std::int64_t> endDate{};
   std::int64_t createdAt{};
   std::int64_t updatedAt{};
};

struct TradeRecord {
   std::int32_t id{};
   std::int32_t tradeGroupId{};
   std::string tradeId{};
   std::string exchange{};
   std::string extraIdentifier{};
   AssetCategory assetCategory{};
   std::string assetName{};
   double quantity{};

   std::optional<double> openExpectedPrice{};
   std::optional<double> openAvgPrice{};
   std::optional<double> openStartFillPrice{};
   std::optional<double> openEndFillPrice{};

   std::optional<double> closeExpectedPrice{};
   std::optional<double> closeAvgPrice{};
   std::optional<double> closeStartFillPrice{};
   std::optional<double> closeEndFillPrice{};

   std::optional<std::int64_t> timeEntry{};
   std::optional<std::int64_t> timeExit{};

   std::optional<double> feeEntry{};
   std::optional<double> feeTotal{};
   std::optional<double> profitTotal{};

   std::int64_t createdAt{};
   std::int64_t updatedAt{};
};

struct TradeWriterCfg {
   std::string filePath{};
   std::string connectionString{};
};

// --- Interface ---
struct ITradeRW {
   enum class TradeRWId : std::int32_t {
      TradeRW
   };

   virtual ~ITradeRW() = default;

   virtual void configure(const TradeWriterCfg& cfg) = 0;

   /**
    * Returns version of ITradeRW module
    * @return version string i.e. 1.0.0
    */
   [[nodiscard]] virtual std::string version() const = 0;

   /**
    * Write Trade to json file and/or into the PostgresSQL Database
    * @param trade
    */
   virtual void write(const TradeRecord& trade) const = 0;

   /**
    * Read Trade from json file or from the PostgresSQL Database
    * @param tradeId
    */
   [[nodiscard]] virtual TradeRecord read(std::int32_t tradeId) const = 0;
};
}  // namespace vk

#endif  // INCLUDE_VK_INTERFACE_I_TRADE_RW_H
