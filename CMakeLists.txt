cmake_minimum_required(VERSION 3.16)
project(vk_common)

set(CMAKE_CXX_STANDARD 20)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00 /bigobj)
else ()
    add_definitions(-fPIC)
endif ()

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

option(MODULE_MANAGER "Add Module Manager" OFF)

if (MODULE_MANAGER)
    find_package(Boost 1.83 REQUIRED COMPONENTS system)
    include_directories(include SYSTEM ${Boost_INCLUDE_DIR})

    set(SOURCES_M_MANAGER
            src/module_factory.cpp
            src/module_manager.cpp)
else ()
    include_directories(include)
endif ()

set(HEADERS
        include/vk/interface/i_json.h
        include/vk/interface/i_exchange_connector.h
        include/vk/interface/i_module_factory.h
        include/vk/interface/i_exchange_downloader.h
        include/vk/utils/utils.h
        include/vk/utils/log_utils.h
        include/vk/utils/json_utils.h
        include/vk/utils/semaphore.h
        include/date.h
        include/base64.h)

set(SOURCES
        src/id_generator.cpp
        src/registry.cpp
        src/utils.cpp
        src/base64.cpp)

if (MODULE_MANAGER)
    add_library(vk_common ${SOURCES} ${SOURCES_M_MANAGER} ${HEADERS})
    target_link_libraries(vk_common Boost::system)
else ()
    add_library(vk_common ${SOURCES} ${HEADERS})
endif ()