// sub_logger.cpp
//
// Demonstrates sub-loggers (nested pipelines) that receive a subset of log
// entries from a parent logger, each with independent filters, enrichers,
// and sinks.  This enables complex routing beyond tag-based filtering.
//
// Compile: g++ -std=c++11 -I include examples/sub_logger.cpp -o sub_logger -pthread

#include "lunar_log.hpp"
#include <lunar_log/sink/sub_logger_sink.hpp>
#include <iostream>

int main() {
    // --- Example 1: Error-alert sub-logger ---
    //
    // The parent logs everything to console and app.log.
    // A sub-logger routes ERROR+ entries to a dedicated error file
    // with extra metadata attached by sub-logger-specific enrichers.
    {
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::DEBUG)
            .writeTo<minta::ConsoleSink>("console")
            .writeTo<minta::FileSink>("app-log", "sub_logger_app.log")
            .subLogger("errors", [](minta::SubLoggerConfiguration& sub) {
                sub.filter("ERROR+")
                   .enrich(minta::Enrichers::property("pipeline", "error-alerts"))
                   .enrich(minta::Enrichers::property("team", "on-call"))
                   .writeTo<minta::FileSink>("sub_logger_errors.log");
            })
            .build();

        logger.debug("Application starting up");
        logger.info("Server listening on port 8080");
        logger.warn("Slow query detected: 2.3s");
        logger.error("Database connection lost");
        logger.fatal("Out of memory — shutting down");

        logger.flush();
        std::cout << "Example 1 complete. Check sub_logger_app.log and sub_logger_errors.log\n";
    }

    // --- Example 2: Audit trail sub-logger ---
    //
    // A sub-logger filters entries containing "audit" and writes them
    // as JSON for machine processing, while the parent uses human-readable
    // output on the console.
    {
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::TRACE)
            .writeTo<minta::ConsoleSink>("console")
            .subLogger("audit", [](minta::SubLoggerConfiguration& sub) {
                sub.filter("INFO+ ~audit")
                   .enrich(minta::Enrichers::property("compliance", "SOC2"))
                   .writeTo<minta::FileSink, minta::JsonFormatter>(
                       "sub_logger_audit.json.log");
            })
            .build();

        logger.info("Regular operation");
        logger.info("audit: user alice logged in from 10.0.0.1");
        logger.warn("audit: permission escalation attempted by bob");
        logger.debug("audit: low-priority debug audit");  // filtered by INFO+
        logger.error("audit: unauthorized data export");

        logger.flush();
        std::cout << "Example 2 complete. Check sub_logger_audit.json.log\n";
    }

    // --- Example 3: Multiple sub-loggers ---
    //
    // Fan out log entries to multiple independent sub-pipelines,
    // each with different filter/enricher/sink configurations.
    {
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::TRACE)
            .writeTo<minta::ConsoleSink>("console")
            .subLogger("errors", [](minta::SubLoggerConfiguration& sub) {
                sub.filter("ERROR+")
                   .enrich(minta::Enrichers::property("severity", "critical"))
                   .writeTo<minta::FileSink>("sub_logger_critical.log");
            })
            .subLogger("debug", [](minta::SubLoggerConfiguration& sub) {
                sub.minLevel(minta::LogLevel::DEBUG)
                   .filterRule("level <= DEBUG")
                   .writeTo<minta::FileSink>("sub_logger_debug.log");
            })
            .build();

        logger.trace("trace message — too low for debug sub-logger");
        logger.debug("debug diagnostics");
        logger.info("normal operation");
        logger.error("critical failure");

        logger.flush();
        std::cout << "Example 3 complete. Check sub_logger_critical.log and sub_logger_debug.log\n";
    }

    return 0;
}
