include(FetchContent)

FetchContent_Declare(
        benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

set(BENCH_SOURCES
        bench_throughput.cpp
        bench_formatting.cpp
        bench_filtering.cpp
        bench_sinks.cpp
        bench_enrichers.cpp
        bench_e2e.cpp
)

add_executable(lunar_log_bench ${BENCH_SOURCES})

target_include_directories(lunar_log_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lunar_log_bench PRIVATE
        LunarLog
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
)

set_target_properties(lunar_log_bench PROPERTIES
        CXX_STANDARD ${LUNARLOG_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(lunar_log_bench PRIVATE /W4)
else()
    target_compile_options(lunar_log_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()
