/**
ID Generator

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_UTILS_ID_GENERATOR_H
#define INCLUDE_VK_UTILS_ID_GENERATOR_H

#include <atomic>

namespace vk {
class IdGenerator32 {
    static std::atomic<std::int32_t> s_currentId;

public:
    static std::int32_t currentId();

    static std::int32_t nextId();
};

class IdGenerator64 {
    static std::atomic<std::int64_t> s_currentId;

public:
    static std::int64_t currentId();

    static std::int64_t nextId();
};
}
#endif // INCLUDE_VK_UTILS_ID_GENERATOR_H
