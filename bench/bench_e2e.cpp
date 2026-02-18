#include <benchmark/benchmark.h>
#include <chrono>
#include <cstdio>
#include <limits>
#include <string>
#include "lunar_log.hpp"
#include "null_sink.hpp"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define BENCH_GETPID() _getpid()
static std::string tempDir() {
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    return std::string(buf);
}
#else
#include <unistd.h>
#define BENCH_GETPID() getpid()
static std::string tempDir() { return "/tmp/"; }
#endif

static std::string benchPath(const std::string& suffix) {
    return tempDir() + "lunar_bench_" + std::to_string(BENCH_GETPID()) + "_" + suffix;
}

// Returns the path of the n-th rotation file.
// e.g. base="/tmp/foo.log", n=1 → "/tmp/foo.001.log"
static std::string rotationPath(const std::string& base, int n) {
    size_t dotPos = base.rfind('.');
    std::string stem = (dotPos != std::string::npos) ? base.substr(0, dotPos) : base;
    std::string ext  = (dotPos != std::string::npos) ? base.substr(dotPos) : "";
    char buf[8];
    std::snprintf(buf, sizeof(buf), ".%03d", n);
    return stem + buf + ext;
}

// ---------------------------------------------------------------------------
// BM_E2E_Realistic
// Production-like configuration measuring the full pipeline:
//   - 3 sinks: NullSink (console stand-in) + JSON file + Rolling error file
//   - 2 enrichers: ThreadId + Property
//   - WARN+ global filter
//   - Tag routing on error sink
// flush() is called inside the loop to measure true end-to-end latency
// (enqueue + consumer processing + all sink writes) per iteration.
// ---------------------------------------------------------------------------
static void BM_E2E_Realistic(benchmark::State& state) {
    std::string jsonPath = benchPath("e2e.log");
    std::string errPath  = benchPath("e2e_errors.log");
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);

        logger.addCustomSink("console", minta::detail::make_unique<minta::NullSink>());
        logger.addSink<minta::FileSink, minta::JsonFormatter>("json-out", jsonPath);
        logger.addSink<minta::RollingFileSink>(
                "errors",
                minta::RollingPolicy::daily(errPath).maxFiles(5));

        logger.enrich(minta::Enrichers::threadId());
        logger.enrich(minta::Enrichers::property("service", "bench-api"));

        logger.addFilterRule("level >= WARN");
        logger.sink("errors").only("error");

        logger.setRateLimit(std::numeric_limits<size_t>::max(),
                            std::chrono::milliseconds(1000));

        for (auto _ : state) {
            logger.warn("[error] Database connection failed for host {host}", "db-01");
            logger.flush();
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations());
    }
    std::remove(jsonPath.c_str());
    std::remove(errPath.c_str());
    for (int i = 1; i <= 5; ++i)
        std::remove(rotationPath(errPath, i).c_str());
}
BENCHMARK(BM_E2E_Realistic);
