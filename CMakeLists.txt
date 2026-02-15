cmake_minimum_required(VERSION 3.10)
project(LunarLog VERSION 1.0.0 LANGUAGES CXX)

set(LUNARLOG_CXX_STANDARD "11" CACHE STRING "C++ standard to use (11, 14, or 17)")
set_property(CACHE LUNARLOG_CXX_STANDARD PROPERTY STRINGS 11 14 17)

# Allow old cmake_minimum_required in fetched dependencies (CMake 4.x compat)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Add Google Test
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

add_library(LunarLog INTERFACE)
target_include_directories(LunarLog INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (WIN32)
    target_link_libraries(LunarLog INTERFACE advapi32)
endif ()

# Examples
add_executable(BasicUsage examples/basic_usage.cpp)
target_link_libraries(BasicUsage PRIVATE LunarLog)
set_target_properties(BasicUsage PROPERTIES
        CXX_STANDARD ${LUNARLOG_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Tests
enable_testing()

set(TEST_SOURCES
        test/test_main.cpp
        test/tests/test_basic_logging.cpp
        test/tests/test_log_levels.cpp
        test/tests/test_rate_limiting.cpp
        test/tests/test_placeholder_validation.cpp
        test/tests/test_escaped_brackets.cpp
        test/tests/test_multiple_sinks.cpp
        test/tests/test_custom_formatter.cpp
        test/tests/test_json_formatter.cpp
        test/tests/test_xml_formatter.cpp
        test/tests/test_context_capture.cpp
        test/tests/test_suffix_formatting.cpp
        test/tests/test_thread_safety.cpp
        test/tests/utils/test_utils.cpp
)

add_executable(TestLunarLog ${TEST_SOURCES})
target_link_libraries(TestLunarLog PRIVATE
        LunarLog
        gtest
        Threads::Threads
)

target_include_directories(TestLunarLog PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/test
)

# Set C++ standard for TestLunarLog
set_target_properties(TestLunarLog PROPERTIES
        CXX_STANDARD ${LUNARLOG_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

include(GoogleTest)
gtest_discover_tests(TestLunarLog)