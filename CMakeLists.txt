cmake_minimum_required(VERSION 3.10)
project(LunarLog VERSION 1.0.0 LANGUAGES CXX)

set(LUNARLOG_CXX_STANDARD "11" CACHE STRING "C++ standard to use (11, 14, or 17)")
set_property(CACHE LUNARLOG_CXX_STANDARD PROPERTY STRINGS 11 14 17)

set(CMAKE_CXX_STANDARD ${LUNARLOG_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to use separate includes and implementation
option(LUNARLOG_USE_SEPARATE_INCLUDE "Use separate includes and implementation of LunarLog" ON)
option(LUNARLOG_USE_SINGLE_INCLUDE_WITH_JSON "Use single include version of LunarLog with JSON implementation" OFF)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add nlohmann/json
FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/archive/v3.11.3.tar.gz
)

FetchContent_MakeAvailable(googletest json)

# Get the path to nlohmann/json include directory
get_target_property(JSON_INCLUDE_DIR nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)

add_library(LunarLog INTERFACE)
target_include_directories(LunarLog INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${JSON_INCLUDE_DIR}
)

if(WIN32)
    target_link_libraries(LunarLog INTERFACE advapi32)
endif()

# Examples
add_executable(BasicUsage examples/basic_usage.cpp)
target_link_libraries(BasicUsage PRIVATE LunarLog nlohmann_json::nlohmann_json)

# Tests
enable_testing()
add_executable(TestLunarLog
        tests/test_main.cpp
        tests/test_lunar_log.cpp
)
target_link_libraries(TestLunarLog PRIVATE
        LunarLog
        gtest
        gtest_main
        nlohmann_json::nlohmann_json
        pthread
)

# Add compile definitions for test output
target_compile_definitions(TestLunarLog PRIVATE
        GTEST_COUT=std::cerr
)

include(GoogleTest)
gtest_discover_tests(TestLunarLog)