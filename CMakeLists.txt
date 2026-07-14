cmake_minimum_required(VERSION 4.0)
project(stonky_common)

# C++23: the chasing-limit executor core (now part of this lib) uses
# std::jthread / std::stop_token.
set(CMAKE_CXX_STANDARD 23)

if (MSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00 /bigobj /utf-8)
else ()
    add_definitions(-fPIC)
endif ()

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

option(MODULE_MANAGER "Add Module Manager" OFF)

if (MODULE_MANAGER)
    find_package(Boost 1.88 REQUIRED COMPONENTS system filesystem)
    include_directories(include SYSTEM ${Boost_INCLUDE_DIR})

    set(SOURCES_M_MANAGER
            src/module_factory.cpp
            src/module_manager.cpp)
else ()
    include_directories(include)
endif ()

find_package(spdlog CONFIG REQUIRED)

set(HEADERS
        include/stonky/interface/i_json.h
        include/stonky/interface/i_exchange_connector.h
        include/stonky/interface/i_execution_gateway.h
        include/stonky/interface/i_module_factory.h
        include/stonky/interface/i_exchange_downloader.h
        include/stonky/interface/i_demo_exchange_connector.h
        include/stonky/interface/i_trade_rw.h
        include/stonky/common/chasing_limit_executor.h
        include/stonky/utils/utils.h
        include/stonky/utils/log_utils.h
        include/stonky/utils/json_utils.h
        include/stonky/utils/semaphore.h
        include/date.h
        include/base64.h)

set(SOURCES
        src/id_generator.cpp
        src/registry.cpp
        src/utils.cpp
        src/base64.cpp
        src/chasing_limit_executor.cpp)

if (MODULE_MANAGER)
    add_library(stonky_common ${SOURCES} ${SOURCES_M_MANAGER} ${HEADERS})
    target_link_libraries(stonky_common Boost::system Boost::filesystem spdlog::spdlog_header_only)
else ()
    add_library(stonky_common ${SOURCES} ${HEADERS})
    target_link_libraries(stonky_common spdlog::spdlog_header_only)
endif ()

target_include_directories(stonky_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Propagate C++23 to every consumer (the executor's jthread/stop_token are in a
# PUBLIC header), so connectors/bots inherit it without each setting the standard.
target_compile_features(stonky_common PUBLIC cxx_std_23)