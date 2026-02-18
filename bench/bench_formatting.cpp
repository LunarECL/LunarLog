#include <benchmark/benchmark.h>
#include <limits>
#include <string>
#include <vector>
#include "lunar_log.hpp"
#include "null_sink.hpp"

// ---------------------------------------------------------------------------
// BM_FormatSimple
// Single placeholder: "Hello {name}"
// Isolates formatting cost only (NullSink, no file I/O, no filter evaluation).
// Compare with BM_LogInfo_SingleThread which includes the full pipeline with
// identical template but measures end-to-end throughput rather than format cost.
// ---------------------------------------------------------------------------
static void BM_FormatSimple(benchmark::State& state) {
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
BENCHMARK(BM_FormatSimple);

// ---------------------------------------------------------------------------
// BM_FormatComplex
// Multiple placeholders with format specifiers and pipe transforms:
// "{method} {path} {status:04} in {elapsed:.2f}ms [{region|upper}]"
// After the first iteration the template is cache-warm; this benchmark
// therefore measures steady-state formatting cost for complex templates.
// ---------------------------------------------------------------------------
static void BM_FormatComplex(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("{method} {path} {status:04} in {elapsed:.2f}ms [{region|upper}]",
                    "GET", "/api/users", 200, 12.34, "us-east-1");
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FormatComplex);

// ---------------------------------------------------------------------------
// BM_FormatCacheHit
// Explicitly pre-warms the template cache before the timed loop, then
// measures the cache-hit path only.  Default cache size is 128 entries.
// ---------------------------------------------------------------------------
static void BM_FormatCacheHit(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    // Pre-warm: parse and cache the template before timing starts.
    logger.info("{method} {path} {status:04} in {elapsed:.2f}ms [{region|upper}]",
                "GET", "/api/users", 200, 12.34, "us-east-1");
    logger.flush();

    for (auto _ : state) {
        logger.info("{method} {path} {status:04} in {elapsed:.2f}ms [{region|upper}]",
                    "GET", "/api/users", 200, 12.34, "us-east-1");
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FormatCacheHit);

// ---------------------------------------------------------------------------
// BM_FormatCacheMiss
// Rotates through N distinct templates (N > default cache capacity of 128)
// so that most lookups are cache misses requiring a full template re-parse.
// With N=256 and a 128-entry cache: entries 0-127 are cached on first use
// and the remaining 128 templates are parsed on every access, giving a ~50 %
// miss rate per rotation cycle.
// ---------------------------------------------------------------------------
static void BM_FormatCacheMiss(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    static constexpr size_t N = 256;
    std::vector<std::string> templates;
    templates.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        templates.push_back("Request_" + std::to_string(i) +
                            " {method} {path} {status:04}");
    }

    size_t idx = 0;
    for (auto _ : state) {
        logger.info(templates[idx % N], "GET", "/api", 200);
        ++idx;
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
    state.SetLabel("256 rotating templates vs 128 cache");
}
BENCHMARK(BM_FormatCacheMiss);

// ---------------------------------------------------------------------------
// BM_FormatPipeTransform
// Pipe chain: "{value|upper|trim|quote}"
// ---------------------------------------------------------------------------
static void BM_FormatPipeTransform(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("{value|upper|trim|quote}", "  hello world  ");
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FormatPipeTransform);

// ---------------------------------------------------------------------------
// BM_FormatIndexed
// Indexed placeholders with reuse: "{0} bought {1} for {0}"
// ---------------------------------------------------------------------------
static void BM_FormatIndexed(benchmark::State& state) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(minta::detail::make_unique<minta::NullSink>());
    logger.setRateLimit(std::numeric_limits<size_t>::max(),
                        std::chrono::milliseconds(1000));

    for (auto _ : state) {
        logger.info("{0} bought {1} for {0}", "alice", 42);
        benchmark::ClobberMemory();
    }
    logger.flush();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FormatIndexed);
