/**
Module manager contains module factories of all loaded modules and allows accessing
modules functionality through them. It is a Singleton.

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/tools/module_manager.h"
#include <boost/dll/import.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <regex>
#include <filesystem>

namespace vk {
ModuleManager::~ModuleManager() {
    m_moduleFactories.clear();
    m_libraries.clear();
}

void ModuleManager::start(const std::string& searchPath) {
    std::lock_guard lock(m_mutex);

    m_moduleFactories.clear();
    m_libraries.clear();

    const std::regex sharedLibraryFilter("\\.dll");
    std::filesystem::path path;

    if (searchPath.empty())
        path = boost::dll::program_location().c_str();
    else
        path = searchPath;

    if (std::filesystem::is_regular_file(path))
        path = path.parent_path();

    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        for (auto& entry : std::filesystem::directory_iterator(path)) {
            if (!std::filesystem::is_regular_file(entry.status()))
                continue;

            const auto pathStr = entry.path().extension().string();
            if (std::smatch what; !std::regex_match(pathStr, what, sharedLibraryFilter))
                continue;

            boost::dll::shared_library lib;
            lib.load(entry.path().c_str(), boost::dll::load_mode::append_decorations);

            if (lib.is_loaded() && lib.has("getModuleFactory")) {
                if (GetFactoryProc* factoryProcedure = lib.get<GetFactoryProc>("getModuleFactory")) {
                    const auto factory = reinterpret_cast<ModuleFactory*>(factoryProcedure());
                    m_moduleFactories.push_back(std::unique_ptr<ModuleFactory>(factory));
                }

                m_libraries.push_back(lib);
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
}
