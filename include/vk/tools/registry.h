/**
Win Registry Reader/Writer

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_TOOLS_REGISTRY_H
#define INCLUDE_VK_TOOLS_REGISTRY_H

#if defined _WIN32
#include <windows.h>

namespace vk {

bool createRegistryKey(HKEY hKeyParent, const char *subKey);

bool writeInRegistry(HKEY hKeyParent, const char *subKey, const char *valueName, DWORD data);

bool readDwordValueRegistry(HKEY hKeyParent, const char *subKey, const char *valueName, DWORD *readData);

}
#endif
#endif // INCLUDE_VK_TOOLS_REGISTRY_H
