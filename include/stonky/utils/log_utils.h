/**
Enum Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@stonky.cz>, Stonky s.r.o.
*/

#ifndef INCLUDE_STONKY_UTILS_LOG_UTILS_H
#define INCLUDE_STONKY_UTILS_LOG_UTILS_H

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

namespace stonky {
enum class LogSeverity : int {
    Info,
    Warning,
    Critical,
    Error,
    Debug,
    Trace
};
}

using onLogMessage = std::function<void(stonky::LogSeverity severity, const std::string& errmsg)>;

inline void defaultLogFunction(const stonky::LogSeverity severity, const std::string& errmsg) {
    switch (severity) {
        case stonky::LogSeverity::Info:
#ifdef VERBOSE_LOG
            spdlog::info(errmsg);
#endif
        break;
        case stonky::LogSeverity::Warning:
            spdlog::warn(errmsg);
        break;
        case stonky::LogSeverity::Critical:
            spdlog::critical(errmsg);
        break;
        case stonky::LogSeverity::Error:
            spdlog::error(errmsg);
        break;
        case stonky::LogSeverity::Debug:
            spdlog::debug(errmsg);
        break;
        case stonky::LogSeverity::Trace:
            spdlog::trace(errmsg);
        break;
    }
}

#endif // INCLUDE_STONKY_UTILS_LOG_UTILS_H
