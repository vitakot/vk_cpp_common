// magic_enum_wrapper.hpp
// Cross-platform wrapper for magic_enum library
// Handles different include paths across Windows/vcpkg, Ubuntu 24.04, and Ubuntu 25.10

#pragma once

#if __has_include("magic_enum/magic_enum.hpp")
    #include "magic_enum/magic_enum.hpp"
#elif __has_include("magic_enum.hpp")
    #include "magic_enum.hpp"
#else
    #error "Cannot find magic_enum header. Please install libmagic-enum-dev or add magic_enum via vcpkg."
#endif
