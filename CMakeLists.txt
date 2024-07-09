cmake_minimum_required(VERSION 3.10)
project(LunarLog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to use separate includes and implementation
option(LUNARLOG_USE_SEPARATE_INCLUDE "Use separate includes and implementation of LunarLog" OFF)
option(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON "Use single include version of LunarLog with JSON implementation" OFF)

if(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON)
    add_library(LunarLog INTERFACE)
    target_include_directories(LunarLog INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
    add_definitions(-DLUNARLOG_WITH_JSON)
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

# Examples
add_executable(BasicUsage examples/basic_usage.cpp)
if(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON)
    target_include_directories(BasicUsage PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
    add_definitions(-DLUNARLOG_WITH_JSON)
elseif(LUNARLOG_USE_SEPARATE_INCLUDE)
    target_include_directories(BasicUsage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
    target_include_directories(BasicUsage PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
endif()
target_link_libraries(BasicUsage PRIVATE LunarLog)

# Tests
enable_testing()
add_executable(TestLunarLog tests/test_lunar_log.cpp)
if(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON)
    target_include_directories(TestLunarLog PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
    add_definitions(-DLUNARLOG_WITH_JSON)
elseif(LUNARLOG_USE_SEPARATE_INCLUDE)
    target_include_directories(TestLunarLog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
    target_include_directories(TestLunarLog PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/single_include
    )
endif()
target_link_libraries(TestLunarLog PRIVATE LunarLog)
add_test(NAME TestLunarLog COMMAND TestLunarLog)
