/**
Module factory - utility for dynamic shared libraries loading

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_COMMON_MODULE_FACTORY_H
#define INCLUDE_VK_COMMON_MODULE_FACTORY_H

#include "vk/interface/i_module_factory.h"
#include <map>
#include <functional>
#include <boost/type_index.hpp>
#include <boost/any.hpp>
#include <mutex>
#include <memory>

namespace vk {
class ModuleFactory final : public IModuleFactory {
    FactoryInfo m_factoryInfo;
    std::map<std::string, boost::any> m_factoryMap;
    std::recursive_mutex m_mutex;

public:
    explicit ModuleFactory(const FactoryInfo &info);

    ~ModuleFactory() override = default;

    void factoryInfo(FactoryInfo &info) const override;

    template<typename T>
    void registerClass(const std::function<std::shared_ptr<T>()> &makeFunc) {
        std::lock_guard lock(m_mutex);

        std::string classId = boost::typeindex::type_id<T>().pretty_name() +
                              boost::typeindex::type_id<decltype(makeFunc)>().pretty_name();

        if (!m_factoryMap.contains(classId)) {
            m_factoryMap.insert_or_assign(classId, makeFunc);
        }
    }

    template<typename T>
    void registerClassByName(const std::string &name, const std::function<std::shared_ptr<T>()> &makeFunc) {
        std::lock_guard lock(m_mutex);

        if (!m_factoryMap.contains(name)) {
            m_factoryMap.insert_or_assign(name, makeFunc);
        }
    }

    template<typename T, typename... Args>
    void registerClass(const std::function<std::shared_ptr<T>(Args &&... args)> &makeFunc) {
        std::lock_guard lock(m_mutex);

        std::string classId = boost::typeindex::type_id<T>().pretty_name() +
                              boost::typeindex::type_id<decltype(makeFunc)>().pretty_name();

        if (!m_factoryMap.contains(classId)) {
            m_factoryMap.insert_or_assign(classId, makeFunc);
        }
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> create(Args &&... args) {
        std::lock_guard lock(m_mutex);

        const std::string classId = boost::typeindex::type_id<T>().pretty_name() +
                                    boost::typeindex::type_id<std::function<std::shared_ptr<T>(Args
                                        &&... arguments)> >().pretty_name();

        const auto it = m_factoryMap.find(classId);

        std::function<std::shared_ptr<T>(Args &&... arguments)> makeFunc = nullptr;

        if (it != m_factoryMap.end()) {
            try {
                makeFunc = boost::any_cast<std::function<std::shared_ptr<T>(Args &&... arguments)> >(it->second);
            } catch (const boost::bad_any_cast &) {
                return nullptr;
            }

            return std::shared_ptr<T>(makeFunc(std::forward<Args>(args)...));
        }

        return nullptr;
    }

    template<typename T, typename... Args>
    std::shared_ptr<T> createByName(const std::string &name, Args &&... args) {
        std::lock_guard lock(m_mutex);

        const auto it = m_factoryMap.find(name);

        std::function<std::shared_ptr<T>(Args &&... arguments)> makeFunc = nullptr;

        if (it != m_factoryMap.end()) {
            try {
                makeFunc = boost::any_cast<std::function<std::shared_ptr<T>(Args &&... arguments)> >(it->second);
            } catch (const boost::bad_any_cast &) {
                return nullptr;
            }

            return std::shared_ptr<T>(makeFunc(std::forward<Args>(args)...));
        }

        return nullptr;
    }

    void finalize() override;
};

extern ModuleFactory *g_moduleFactory;
}

#endif // INCLUDE_VK_COMMON_MODULE_FACTORY_H
