cmake_minimum_required(VERSION 3.16)
project(vk_tools)

set(CMAKE_CXX_STANDARD 20)

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

set(HEADERS include/vk/tools/i_json.h
        include/vk/tools/utils.h
        include/vk/tools/log_utils.h
        include/vk/tools/json_utils.h
        include/vk/tools/i_exchange_connector.h
        include/vk/tools/semaphore.h)

set(SOURCES
        src/id_generator.cpp
        src/registry.cpp
        src/utils.cpp)

if (MODULE_MANAGER)
    add_library(vk_tools ${SOURCES} ${SOURCES_M_MANAGER} ${HEADERS})
    target_link_libraries(vk_tools Boost::system)
else ()
    add_library(vk_tools ${SOURCES} ${HEADERS})
endif ()