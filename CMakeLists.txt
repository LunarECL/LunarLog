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
    set(EXAMPLE_TARGETS
            BasicUsage
            FormatSpecifiers
            Operators
            Filtering
            CultureFormatting
            CustomSink
            CustomFormatter
            ContextCapture
            StructuredOutput
            MultiSink
            RateLimiting
            ThreadSafety
            NamedSinks
            TagRouting
            PipeTransforms
            OutputTemplate
            ExceptionAttachment
            Enrichers
    )

    set(EXAMPLE_SOURCES
            examples/basic_usage.cpp
            examples/format_specifiers.cpp
            examples/operators.cpp
            examples/filtering.cpp
            examples/culture_formatting.cpp
            examples/custom_sink.cpp
            examples/custom_formatter.cpp
            examples/context_capture.cpp
            examples/structured_output.cpp
            examples/multi_sink.cpp
            examples/rate_limiting.cpp
            examples/thread_safety.cpp
            examples/named_sinks.cpp
            examples/tag_routing.cpp
            examples/pipe_transforms.cpp
            examples/output_template.cpp
            examples/exception_attachment.cpp
            examples/enrichers.cpp
    )

    list(LENGTH EXAMPLE_TARGETS EXAMPLE_COUNT)
    math(EXPR EXAMPLE_LAST "${EXAMPLE_COUNT} - 1")

    foreach(idx RANGE ${EXAMPLE_LAST})
        list(GET EXAMPLE_TARGETS ${idx} TARGET_NAME)
        list(GET EXAMPLE_SOURCES ${idx} SOURCE_FILE)

        add_executable(${TARGET_NAME} ${SOURCE_FILE})
        target_link_libraries(${TARGET_NAME} PRIVATE LunarLog Threads::Threads)
        set_target_properties(${TARGET_NAME} PROPERTIES
                CXX_STANDARD ${LUNARLOG_CXX_STANDARD}
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF
        )
        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE /W4)
        else()
            target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
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
            test/tests/test_filtering.cpp
            test/tests/test_culture_formatting.cpp
            test/tests/test_named_sinks.cpp
            test/tests/test_tag_routing.cpp
            test/tests/test_pipe_transforms.cpp
            test/tests/test_scoped_context.cpp
            test/tests/test_compact_filter.cpp
            test/tests/test_rolling_file.cpp
            test/tests/test_output_template.cpp
            test/tests/test_indexed_parameters.cpp
            test/tests/test_alignment_padding.cpp
            test/tests/test_compact_json_formatter.cpp
            test/tests/test_exception_attachment.cpp
            test/tests/test_enrichers.cpp
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