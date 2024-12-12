/**
Module manager contains module factories of all loaded modules and allows accessing
modules functionality through them. It is a Singleton.

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/tools/module_manager.h"
#include <boost/range/adaptors.hpp>
#include <boost/dll/import.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

namespace vk {
ModuleManager::~ModuleManager() {
    m_moduleFactories.clear();
    m_libraries.clear();
}

void ModuleManager::start(const std::string& searchPath) {
    std::lock_guard lock(m_mutex);

    m_moduleFactories.clear();
    m_libraries.clear();

    const boost::regex sharedLibraryFilter("\\.dll");
    boost::filesystem::path path;

    if (searchPath.empty())
        path = boost::dll::program_location();
    else
        path = searchPath;

    if (boost::filesystem::is_regular_file(path))
        path = path.parent_path();

    if (boost::filesystem::exists(path) && boost::filesystem::is_directory(path)) {
        for (auto& entry : boost::filesystem::directory_iterator(path)) {
            if (!boost::filesystem::is_regular_file(entry.status()))
                continue;

            if (boost::smatch what; !boost::regex_match(entry.path().extension().string(), what, sharedLibraryFilter))
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
