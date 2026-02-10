/**
Exchange Data Downloader Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_I_EXCHANGE_DOWNLOADER_H
#define INCLUDE_VK_INTERFACE_I_EXCHANGE_DOWNLOADER_H

#include "exchange_enums.h"
#include <string>
#include <vector>
#include <functional>

namespace vk {
using onSymbolsToUpdate = std::function<void(const std::vector<std::string>& symbols)>;
using onSymbolCompleted = std::function<void(const std::string& symbol)>;

struct IExchangeDownloader {
    virtual ~IExchangeDownloader() = default;

    virtual void updateMarketData(const std::string& dirPath,
                                  const std::vector<std::string>& symbols,
                                  CandleInterval candleInterval,
                                  const onSymbolsToUpdate& onSymbolsToUpdateCB,
                                  const onSymbolCompleted& onSymbolCompletedCB,
                                  bool convertToT6) const = 0;

    virtual void updateMarketData(const std::string& connectionString,
                                  const onSymbolsToUpdate& onSymbolsToUpdateCB,
                                  const onSymbolCompleted& onSymbolCompletedCB) const = 0;

    virtual void updateFundingRateData(const std::string& dirPath,
                                       const std::vector<std::string>& symbols,
                                       const onSymbolsToUpdate& onSymbolsToUpdateCB,
                                       const onSymbolCompleted& onSymbolCompletedCB) const = 0;
};
}
#endif //INCLUDE_VK_INTERFACE_I_EXCHANGE_DOWNLOADER_H
