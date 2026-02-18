#include <benchmark/benchmark.h>
#include <cstdio>
#include <limits>
#include <string>
#include "lunar_log.hpp"
#include "null_sink.hpp"

#ifdef _WIN32
#include <process.h>
#define BENCH_GETPID() _getpid()
#else
#include <unistd.h>
#define BENCH_GETPID() getpid()
#endif

static std::string benchPath(const std::string& suffix) {
    return "/tmp/lunar_bench_" + std::to_string(BENCH_GETPID()) + "_" + suffix;
}

// ---------------------------------------------------------------------------
// BM_Sink_Null
// NullSink baseline — minimal sink overhead.
// ---------------------------------------------------------------------------
static void BM_Sink_Null(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("Null sink {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Sink_Null);

// ---------------------------------------------------------------------------
// BM_Sink_File_HumanReadable
// FileSink with default HumanReadableFormatter.
// ---------------------------------------------------------------------------
static void BM_Sink_File_HumanReadable(benchmark::State& state) {
    std::string path = benchPath("hr.log");
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);
        logger.addSink<minta::FileSink>(path);
        logger.setRateLimit(std::numeric_limits<size_t>::max(),
                            std::chrono::milliseconds(1000));

        for (auto _ : state) {
            logger.info("Human-readable {n}", 1);
            benchmark::ClobberMemory();
        }
        logger.flush();
        state.SetItemsProcessed(state.iterations());
    }
    std::remove(path.c_str());
}
BENCHMARK(BM_Sink_File_HumanReadable);

// ---------------------------------------------------------------------------
// BM_Sink_File_Json
// FileSink with JsonFormatter.
// ---------------------------------------------------------------------------
static void BM_Sink_File_Json(benchmark::State& state) {
    std::string path = benchPath("json.log");
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);
        logger.addSink<minta::FileSink, minta::JsonFormatter>(path);
        logger.setRateLimit(std::numeric_limits<size_t>::max(),
                            std::chrono::milliseconds(1000));

        for (auto _ : state) {
            logger.info("JSON sink {n}", 1);
            benchmark::ClobberMemory();
        }
        logger.flush();
        state.SetItemsProcessed(state.iterations());
    }
    std::remove(path.c_str());
}
BENCHMARK(BM_Sink_File_Json);

// ---------------------------------------------------------------------------
// BM_Sink_File_CompactJson
// FileSink with CompactJsonFormatter.
// ---------------------------------------------------------------------------
static void BM_Sink_File_CompactJson(benchmark::State& state) {
    std::string path = benchPath("cjson.log");
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);
        logger.addSink<minta::FileSink, minta::CompactJsonFormatter>(path);
        logger.setRateLimit(std::numeric_limits<size_t>::max(),
                            std::chrono::milliseconds(1000));

        for (auto _ : state) {
            logger.info("Compact JSON {n}", 1);
            benchmark::ClobberMemory();
        }
        logger.flush();
        state.SetItemsProcessed(state.iterations());
    }
    std::remove(path.c_str());
}
BENCHMARK(BM_Sink_File_CompactJson);

// ---------------------------------------------------------------------------
// BM_Sink_Rolling
// RollingFileSink with daily policy.
// ---------------------------------------------------------------------------
static void BM_Sink_Rolling(benchmark::State& state) {
    std::string path = benchPath("rolling.log");
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);
        logger.addSink<minta::RollingFileSink>(
                minta::RollingPolicy::daily(path).maxFiles(5));
        logger.setRateLimit(std::numeric_limits<size_t>::max(),
                            std::chrono::milliseconds(1000));

        for (auto _ : state) {
            logger.info("Rolling sink {n}", 1);
            benchmark::ClobberMemory();
        }
        logger.flush();
        state.SetItemsProcessed(state.iterations());
    }
    std::remove(path.c_str());
}
BENCHMARK(BM_Sink_Rolling);
