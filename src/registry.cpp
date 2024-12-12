/**
Win Registry Reader/Writer

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/tools/registry.h"

namespace vk {
#if defined _WIN32
bool createRegistryKey(HKEY hKeyParent, const char *subKey) {
    DWORD dwDisposition;
    HKEY hKey;
    DWORD Ret;
    Ret =
            RegCreateKeyExA(
                    hKeyParent,
                    subKey,
                    0,
                    nullptr,
                    REG_OPTION_NON_VOLATILE,
                    KEY_WRITE,
                    nullptr,
                    &hKey,
                    &dwDisposition);
    if (Ret != ERROR_SUCCESS) {
        return FALSE;
    }
    RegCloseKey(hKey);
    return TRUE;
}

bool writeInRegistry(HKEY hKeyParent, const char *subKey, const char *valueName, DWORD data) {
    DWORD Ret; //use to check status
    HKEY hKey; //key
    //Open the key
    Ret = RegOpenKeyExA(
            hKeyParent,
            subKey,
            0,
            KEY_WRITE,
            &hKey
    );
    if (Ret == ERROR_SUCCESS) {
        //Set the value in key
        if (ERROR_SUCCESS !=
            RegSetValueExA(
                    hKey,
                    valueName,
                    0,
                    REG_DWORD,
                    reinterpret_cast<BYTE *>(&data),
                    sizeof(data))) {
            RegCloseKey(hKey);
            return FALSE;
        }
        //close the key
        RegCloseKey(hKey);
        return TRUE;
    }
    return FALSE;
}

bool readDwordValueRegistry(HKEY hKeyParent, const char *subKey, const char *valueName, DWORD *readData) {
    HKEY hKey;
    DWORD Ret;
    //Check if the registry exists
    Ret = RegOpenKeyExA(
            hKeyParent,
            subKey,
            0,
            KEY_READ,
            &hKey
    );
    if (Ret == ERROR_SUCCESS) {
        DWORD data;
        DWORD len = sizeof(DWORD);//size of data
        Ret = RegQueryValueExA(
                hKey,
                valueName,
                nullptr,
                nullptr,
                (LPBYTE) (&data),
                &len
        );
        if (Ret == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            (*readData) = data;
            return TRUE;
        }
        RegCloseKey(hKey);
        return FALSE;
    } else {
        return FALSE;
    }
}
#endif
}
