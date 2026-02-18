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
static void BM_LogInfo_MultiThread(benchmark::State& state) {
    // Shared logger — single instance used by all threads to measure
    // shared-logger contention (lock/queue pressure under concurrent writes).
    // C++11 guarantees thread-safe initialisation of function-local statics.
    static auto* logger = []() {
        auto* l = new minta::LunarLog(minta::LogLevel::TRACE, false);
        l->addCustomSink(minta::detail::make_unique<minta::NullSink>());
        l->setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));
        return l;
    }();

    for (auto _ : state) {
        logger->info("Hello {name}", "World");
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_LogInfo_MultiThread)->Threads(1)->Threads(2)->Threads(4)->Threads(8);
