/**
Module manager contains module factories of all loaded modules and allows accessing
modules functionality through them. It is a Singleton.

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/common/module_manager.h"
#include <boost/dll/import.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <regex>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace vk {
ModuleManager::~ModuleManager() {
    m_moduleFactories.clear();
    m_libraries.clear();
}

void ModuleManager::start(const std::string& searchPath) {
    std::lock_guard lock(m_mutex);

    m_moduleFactories.clear();
    m_libraries.clear();

    std::filesystem::path path;

    if (searchPath.empty())
        path = boost::dll::program_location().c_str();
    else
        path = searchPath;

    if (is_regular_file(path))
        path = path.parent_path();

    if (exists(path) && is_directory(path)) {
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (!is_regular_file(entry.status()))
                continue;

            if (const auto extensionStr = entry.path().extension().string();
                extensionStr == ".so" || extensionStr == ".dll") {
                boost::dll::shared_library lib;
                lib.load(entry.path().c_str());

                if (lib.is_loaded()) {
                    spdlog::info("Module library loaded: {}", entry.path().string());
                    if (GetFactoryProc* factoryProcedure = lib.get<GetFactoryProc>("getModuleFactory")) {
                        const auto factory = reinterpret_cast<ModuleFactory*>(factoryProcedure());
                        m_moduleFactories.push_back(std::unique_ptr<ModuleFactory>(factory));
                        FactoryInfo factoryInfo;
                        factory->factoryInfo(factoryInfo);
                        spdlog::info("Module factory loaded: {}, version: {}", factoryInfo.m_description,
                                     factoryInfo.m_version);

                        m_libraries.push_back(lib);
                    }
                    else {
                        spdlog::error("Module factory failed to load: {}", entry.path().string());
                    }
                }
                else {
                    spdlog::error("Module library failed to load: {}", entry.path().string());
                }
            }
        }
    }
}

void ModuleManager::stop() {
    std::lock_guard lock(m_mutex);

    for (const auto& entry : m_moduleFactories) {
        entry->finalize();
    }

    m_moduleFactories.clear();
}

std::vector<FactoryInfo> ModuleManager::getFactoriesInfo() const {
    std::vector<FactoryInfo> retVal;

    for (const auto& entry : m_moduleFactories) {
        FactoryInfo factoryInfo;
        entry->factoryInfo(factoryInfo);
        retVal.push_back(factoryInfo);
    }

    return retVal;
}
}
