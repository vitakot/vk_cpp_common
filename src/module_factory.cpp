/**
Module factory - utility for dynamic shared libraries loading

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/tools/module_factory.h"

namespace vk {
ModuleFactory *g_moduleFactory = nullptr;

ModuleFactory::ModuleFactory(const FactoryInfo &info) {
    m_factoryInfo = info;
}

void ModuleFactory::factoryInfo(FactoryInfo &info) const {
    info = m_factoryInfo;
}

void ModuleFactory::finalize() {
    m_factoryMap.clear();
}
}
