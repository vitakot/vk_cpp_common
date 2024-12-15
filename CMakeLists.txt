cmake_minimum_required(VERSION 3.16)
project(vk_tools)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost 1.78 REQUIRED)

include_directories(include SYSTEM ${Boost_INCLUDE_DIR})

set(HEADERS include/vk/tools/i_json.h
        include/vk/tools/utils.h
        include/vk/tools/log_utils.h
        include/vk/tools/json_utils.h
        include/vk/tools/i_exchange_connector.h
        include/vk/tools/semaphore.h)

set(SOURCES
        src/module_factory.cpp
        src/module_manager.cpp
        src/id_generator.cpp
        src/registry.cpp
        src/utils.cpp)

add_library(vk_tools ${SOURCES} ${HEADERS})