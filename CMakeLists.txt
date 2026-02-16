cmake_minimum_required(VERSION 3.10)
project(LunarLog VERSION 1.0.0 LANGUAGES CXX)

set(LUNARLOG_CXX_STANDARD "11" CACHE STRING "C++ standard to use (11, 14, or 17)")
set_property(CACHE LUNARLOG_CXX_STANDARD PROPERTY STRINGS 11 14 17)

option(LUNARLOG_BUILD_TESTS "Build LunarLog tests" ON)
option(LUNARLOG_BUILD_EXAMPLES "Build LunarLog examples" ON)

find_package(Threads REQUIRED)

add_library(LunarLog INTERFACE)
target_include_directories(LunarLog INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(LunarLog INTERFACE cxx_std_11)

if(LUNARLOG_BUILD_EXAMPLES)
    add_executable(BasicUsage examples/basic_usage.cpp)
    target_link_libraries(BasicUsage PRIVATE LunarLog Threads::Threads)
    set_target_properties(BasicUsage PROPERTIES
            CXX_STANDARD ${LUNARLOG_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(BasicUsage PRIVATE /W4)
    else()
        target_compile_options(BasicUsage PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(LUNARLOG_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

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
            test/tests/test_edge_cases.cpp
            test/tests/test_additional.cpp
            test/tests/test_round7_fixes.cpp
            test/tests/test_operators.cpp
            test/tests/test_template_handling.cpp
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

    set_target_properties(TestLunarLog PROPERTIES
            CXX_STANDARD ${LUNARLOG_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(TestLunarLog PRIVATE /W4)
    else()
        target_compile_options(TestLunarLog PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    include(GoogleTest)
    gtest_discover_tests(TestLunarLog)
endif()