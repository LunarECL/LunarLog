cmake_minimum_required(VERSION 3.10)
project(LunarLog VERSION 1.0.0 LANGUAGES CXX)

set(LUNARLOG_CXX_STANDARD "11" CACHE STRING "C++ standard to use (11, 14, or 17)")
set_property(CACHE LUNARLOG_CXX_STANDARD PROPERTY STRINGS 11 14 17)

set(CMAKE_CXX_STANDARD ${LUNARLOG_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to use separate includes and implementation
option(LUNARLOG_USE_SEPARATE_INCLUDE "Use separate includes and implementation of LunarLog" OFF)
option(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON "Use single include version of LunarLog with JSON implementation" OFF)

if(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON)
    add_library(LunarLog INTERFACE)
    target_include_directories(LunarLog INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
    target_compile_definitions(LunarLog INTERFACE LUNARLOG_WITH_JSON)
elseif(LUNARLOG_USE_SEPARATE_INCLUDE)
    add_library(LunarLog INTERFACE)
    target_include_directories(LunarLog INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
else()
    add_library(LunarLog INTERFACE)
    target_include_directories(LunarLog INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
endif()

if(WIN32)
    target_link_libraries(LunarLog INTERFACE advapi32)
endif()

# Examples
add_executable(BasicUsage examples/basic_usage.cpp)
target_link_libraries(BasicUsage PRIVATE LunarLog)

# Tests
enable_testing()
add_executable(TestLunarLog tests/test_lunar_log.cpp)
target_link_libraries(TestLunarLog PRIVATE LunarLog)
add_test(NAME TestLunarLog COMMAND TestLunarLog)