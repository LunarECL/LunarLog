cmake_minimum_required(VERSION 3.10)
project(LunarLog VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if (WIN32)
    target_link_libraries(LunarLog INTERFACE advapi32)
endif ()

# Examples
add_executable(BasicUsage examples/basic_usage.cpp)
target_link_libraries(BasicUsage PRIVATE LunarLog nlohmann_json::nlohmann_json)

# Tests
enable_testing()
add_executable(TestLunarLog
        tests/test_lunar_log.cpp
        tests/test_main.cpp
)
target_link_libraries(TestLunarLog PRIVATE
        LunarLog
        gtest
        gtest_main
        nlohmann_json::nlohmann_json
        pthread
)

target_include_directories(TestLunarLog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(TestLunarLog)