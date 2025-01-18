/**
ID Generator

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#include "vk/utils/id_generator.h"
#include <chrono>

namespace vk {
std::atomic<std::int64_t> IdGenerator64::s_currentId(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());

std::atomic<std::int32_t> IdGenerator32::s_currentId(
    static_cast<std::int32_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()));

std::int32_t IdGenerator32::currentId() {
    return s_currentId;
}

std::int32_t IdGenerator32::nextId() {
    s_currentId.fetch_add(1);
    return s_currentId;
}

std::int64_t IdGenerator64::currentId() {
    return s_currentId;
}

std::int64_t IdGenerator64::nextId() {
    s_currentId.fetch_add(1);
    return s_currentId;
}
}
