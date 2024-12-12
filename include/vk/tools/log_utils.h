/**
Enum Utilities

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2022 Vitezslav Kot <vitezslav.kot@gmail.com>.
*/

#ifndef INCLUDE_VK_TOOLS_LOG_UTILS_H
#define INCLUDE_VK_TOOLS_LOG_UTILS_H

#include <enum.h>
#include <functional>

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
BETTER_ENUM(LogSeverity, std::int32_t,
            Info,
            Warning,
            Critical,
            Error,
            Debug,
            Trace
)
}

using onLogMessage = std::function<void(vk::LogSeverity severity, const std::string & errmsg)>;

#endif // INCLUDE_VK_TOOLS_LOG_UTILS_H
