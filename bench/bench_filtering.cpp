#include <benchmark/benchmark.h>
#include <limits>
#include "lunar_log.hpp"
#include "null_sink.hpp"

// ---------------------------------------------------------------------------
// BM_Filter_None
// No filters — pure baseline.
// ---------------------------------------------------------------------------
static void BM_Filter_None(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("Baseline {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_None);

// ---------------------------------------------------------------------------
// BM_Filter_MinLevel
// Global min level only — messages pass the check.
// ---------------------------------------------------------------------------
static void BM_Filter_MinLevel(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("Level-filtered {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_MinLevel);

// ---------------------------------------------------------------------------
// BM_Filter_Predicate
// Global predicate filter — every entry evaluated by the lambda.
// ---------------------------------------------------------------------------
static void BM_Filter_Predicate(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    logger.setFilter([](const minta::LogEntry& entry) {
        return entry.level >= minta::LogLevel::INFO;
    });

    for (auto _ : state) {
        logger.info("Predicate-filtered {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_Predicate);

// ---------------------------------------------------------------------------
// BM_Filter_DSL_1Rule
// Single DSL filter rule.
// ---------------------------------------------------------------------------
static void BM_Filter_DSL_1Rule(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    logger.addFilterRule("level >= INFO");

    for (auto _ : state) {
        logger.info("DSL-1 {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_DSL_1Rule);

// ---------------------------------------------------------------------------
// BM_Filter_DSL_5Rules
// Five DSL filter rules — all must pass (AND combination).
// ---------------------------------------------------------------------------
static void BM_Filter_DSL_5Rules(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    logger.addFilterRule("level >= INFO");
    logger.addFilterRule("not message contains 'heartbeat'");
    logger.addFilterRule("not message contains 'debug'");
    logger.addFilterRule("not message contains 'noisy'");
    logger.addFilterRule("not message contains 'ignored'");

    for (auto _ : state) {
        logger.info("DSL-5 {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_DSL_5Rules);

// ---------------------------------------------------------------------------
// BM_Filter_DSL_10Rules
// Ten DSL filter rules.
// ---------------------------------------------------------------------------
static void BM_Filter_DSL_10Rules(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    logger.addFilterRule("level >= INFO");
    logger.addFilterRule("not message contains 'heartbeat'");
    logger.addFilterRule("not message contains 'debug'");
    logger.addFilterRule("not message contains 'noisy'");
    logger.addFilterRule("not message contains 'ignored'");
    logger.addFilterRule("not message contains 'internal'");
    logger.addFilterRule("not message contains 'healthcheck'");
    logger.addFilterRule("not message contains 'polling'");
    logger.addFilterRule("not message contains 'keepalive'");
    logger.addFilterRule("not message contains 'metrics_raw'");

    for (auto _ : state) {
        logger.info("DSL-10 {n}", 1);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_DSL_10Rules);

// ---------------------------------------------------------------------------
// BM_Filter_Compact
// Compact filter syntax (parsed into DSL rules).
// ---------------------------------------------------------------------------
static void BM_Filter_Compact(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    logger.filter("INFO+ ~request !~heartbeat");

    for (auto _ : state) {
        logger.info("Processing request {id}", 42);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_Compact);

// ---------------------------------------------------------------------------
// BM_Filter_TagRouting
// 3 sinks with tag routing: only() and except() configured.
// ---------------------------------------------------------------------------
static void BM_Filter_TagRouting(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    logger.addCustomSink("console", minta::detail::make_unique<minta::NullSink>());
    logger.addCustomSink("auth-log", minta::detail::make_unique<minta::NullSink>());
    logger.addCustomSink("main-log", minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    logger.sink("auth-log").only("auth");
    logger.sink("main-log").except("health");

    for (auto _ : state) {
        logger.info("[auth] User {name} logged in", "alice");
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Filter_TagRouting);
