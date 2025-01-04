/**
Enum Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_TOOLS_LOG_UTILS_H
#define INCLUDE_VK_TOOLS_LOG_UTILS_H

#include <string>
#include <functional>
#include <spdlog/spdlog.h>

#ifndef STRINGIZE_I
#define STRINGIZE_I(x) #x
#endif

#ifndef STRINGIZE
#define STRINGIZE(x) STRINGIZE_I(x)
#endif

#ifndef MAKE_FILELINE
#define MAKE_FILELINE \
    __FILE__ "(" STRINGIZE(__LINE__) ")"
#endif

namespace vk {
enum class LogSeverity : int {
    Info,
    Warning,
    Critical,
    Error,
    Debug,
    Trace
};
}

using onLogMessage = std::function<void(vk::LogSeverity severity, const std::string& errmsg)>;

inline void defaultLogFunction(const vk::LogSeverity severity, const std::string& errmsg) {
    switch (severity) {
        case vk::LogSeverity::Info:
#ifdef VERBOSE_LOG
            spdlog::info(errmsg);
#endif
        break;
        case vk::LogSeverity::Warning:
            spdlog::warn(errmsg);
        break;
        case vk::LogSeverity::Critical:
            spdlog::critical(errmsg);
        break;
        case vk::LogSeverity::Error:
            spdlog::error(errmsg);
        break;
        case vk::LogSeverity::Debug:
            spdlog::debug(errmsg);
        break;
        case vk::LogSeverity::Trace:
            spdlog::trace(errmsg);
        break;
    }
}

#endif // INCLUDE_VK_TOOLS_LOG_UTILS_H
