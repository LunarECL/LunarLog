#include <benchmark/benchmark.h>
#include <limits>
#include "lunar_log.hpp"
#include "null_sink.hpp"

// ---------------------------------------------------------------------------
// BM_EmptyLogger
// Logger with 0 sinks — measures absolute minimum overhead:
// level check, template parse, entry construction, queue push with no consumer.
// ---------------------------------------------------------------------------
static void BM_EmptyLogger(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    // No sinks attached.  The consumer thread never starts, so this captures
    // only the producer-side cost: level gate, template parse, entry alloc.
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("Hello {name}", "World");
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EmptyLogger);

// ---------------------------------------------------------------------------
// BM_LogInfo_SingleThread
// Measures single-thread info-level log throughput with NullSink.
// Includes the full pipeline: parse, format, enqueue, consumer write.
// ---------------------------------------------------------------------------
static void BM_LogInfo_SingleThread(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("Hello {name}", "World");
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogInfo_SingleThread);

// ---------------------------------------------------------------------------
// BM_LogTrace_Disabled
// Trace log with INFO min level.  The level check short-circuits before any
// template parsing, formatting, or queue work — should be near-zero cost.
// ---------------------------------------------------------------------------
static void BM_LogTrace_Disabled(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.trace("This should be very fast {value}", 42);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogTrace_Disabled);

// ---------------------------------------------------------------------------
// BM_LogInfo_MultiThread
// Same info log call measured under increasing thread counts.
// A single shared logger is used across all threads to measure real
// contention on the internal queue and synchronisation primitives.
// ---------------------------------------------------------------------------
// Shared logger — single instance used by all threads to measure
// shared-logger contention (lock/queue pressure under concurrent writes).
// C++11 guarantees thread-safe initialisation of function-local statics.
// Using a static local object (not new) so the destructor runs at program exit.
static minta::LunarLog& sharedLogger() {
    static minta::LunarLog logger(minta::LogLevel::TRACE, false);
    static bool init = [&]() {
        logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
        logger.setRateLimit(std::numeric_limits<size_t>::max(),
                            std::chrono::milliseconds(1000));
        return true;
    }();
    (void)init;
    return logger;
}

static void BM_LogInfo_MultiThread(benchmark::State& state) {
    for (auto _ : state) {
        sharedLogger().info("Hello {name}", "World");
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogInfo_MultiThread)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// ---------------------------------------------------------------------------
// BM_LogInfo_FlushEvery1
// Flush after every single message — measures worst-case end-to-end latency.
// Includes: enqueue + consumer processing + sink write + sync round-trip.
// ---------------------------------------------------------------------------
static void BM_LogInfo_FlushEvery1(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("Flush-every-1 {n}", 1);
        logger.flush();
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogInfo_FlushEvery1);

// ---------------------------------------------------------------------------
// BM_LogInfo_FlushEvery100
// Flush every 100 messages — balanced latency/throughput trade-off.
// ---------------------------------------------------------------------------
static void BM_LogInfo_FlushEvery100(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    size_t count = 0;
    for (auto _ : state) {
        logger.info("Flush-every-100 {n}", count);
        if (++count % 100 == 0) logger.flush();
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogInfo_FlushEvery100);

// ---------------------------------------------------------------------------
// BM_LogInfo_FlushEvery1000
// Flush every 1000 messages — high-throughput batch mode.
// ---------------------------------------------------------------------------
static void BM_LogInfo_FlushEvery1000(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    size_t count = 0;
    for (auto _ : state) {
        logger.info("Flush-every-1000 {n}", count);
        if (++count % 1000 == 0) logger.flush();
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogInfo_FlushEvery1000);
