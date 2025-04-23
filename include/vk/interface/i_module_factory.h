/**
Module Factory Interface

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_INTERFACE_I_MODULE_FACTORY_H
#define INCLUDE_VK_INTERFACE_I_MODULE_FACTORY_H

#include <string>
#include "boost/config.hpp"

namespace vk {
struct FactoryInfo {
    std::string m_id;
    std::string m_description;
};

struct IModuleFactory {
    virtual ~IModuleFactory() = default;

    virtual void factoryInfo(FactoryInfo& info) const = 0;

    virtual void finalize() = 0;
};

extern "C" {
BOOST_SYMBOL_EXPORT IModuleFactory* getModuleFactory();
typedef IModuleFactory*(GetFactoryProc)();
}
}

#endif // INCLUDE_VK_INTERFACE_I_MODULE_FACTORY_H
