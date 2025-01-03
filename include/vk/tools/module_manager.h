/**
Module manager contains module factories of all loaded modules and allows accessing
modules functionality through them. It is a Singleton.

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_TOOLS_MODULE_MANAGER_H
#define INCLUDE_VK_TOOLS_MODULE_MANAGER_H

#include "module_factory.h"
#include <mutex>
#include <list>
#include <boost/dll/shared_library.hpp>

namespace vk {
class ModuleManager {
public:
    static std::shared_ptr<ModuleManager> getInstance() {
        static std::shared_ptr<ModuleManager> instance;

        if (!instance)
            instance = std::shared_ptr<ModuleManager>(new ModuleManager());

        return instance;
    }

private:
    std::recursive_mutex m_mutex{};
    std::list<boost::dll::shared_library> m_libraries{};
    std::list<std::unique_ptr<ModuleFactory> > m_moduleFactories;

    ModuleManager() = default;

public:
    ModuleManager(ModuleManager const &) = delete;

    void operator=(ModuleManager const &) = delete;

    ~ModuleManager();

    void start(const std::string &searchPath = std::string());

    void stop();

    template<typename T, typename... Args>
    std::shared_ptr<T> create(Args &&... args) noexcept {
        for (auto &entry: m_moduleFactories) {
            std::shared_ptr<T> retVal = entry->create<T>(std::forward<Args>(args)...);

            if (retVal != nullptr)
                return retVal;
        }
        assert(nullptr);
        return nullptr;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> createByName(const std::string &name, Args &&... args) noexcept {
        for (auto &entry: m_moduleFactories) {
            std::shared_ptr<T> retVal = entry->createByName<T>(name, std::forward<Args>(args)...);

            if (retVal != nullptr)
                return retVal;
        }
        assert(nullptr);
        return nullptr;
    }
};
}

#endif // INCLUDE_VK_TOOLS_MODULE_MANAGER_H
