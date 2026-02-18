#include <benchmark/benchmark.h>
#include <limits>
#include "lunar_log.hpp"
#include "null_sink.hpp"

// ---------------------------------------------------------------------------
// BM_Enricher_None
// No enrichers — baseline.
// ---------------------------------------------------------------------------
static void BM_Enricher_None(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("No enricher {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Enricher_None);

// ---------------------------------------------------------------------------
// BM_Enricher_ThreadId
// Single ThreadId enricher (dynamic — evaluates per entry).
// ---------------------------------------------------------------------------
static void BM_Enricher_ThreadId(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));
    logger.enrich(minta::Enrichers::threadId());

    for (auto _ : state) {
        logger.info("ThreadId enricher {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Enricher_ThreadId);

// ---------------------------------------------------------------------------
// BM_Enricher_Three
// Three enrichers: ThreadId + ProcessId + Property("env","prod").
// ---------------------------------------------------------------------------
static void BM_Enricher_Three(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));
    logger.enrich(minta::Enrichers::threadId());
    logger.enrich(minta::Enrichers::processId());
    logger.enrich(minta::Enrichers::property("env", "prod"));

    for (auto _ : state) {
        logger.info("Three enrichers {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Enricher_Three);

// ---------------------------------------------------------------------------
// BM_Enricher_Lambda
// Custom lambda enricher.
// ---------------------------------------------------------------------------
static void BM_Enricher_Lambda(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));
    logger.enrich([](minta::LogEntry& entry) {
        entry.customContext["correlationId"] = "bench-corr-id";
    });

    for (auto _ : state) {
        logger.info("Lambda enricher {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Enricher_Lambda);
