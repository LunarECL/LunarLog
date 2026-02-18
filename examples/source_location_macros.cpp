// source_location_macros.cpp
//
// Demonstrates the convenience source-location macros in LunarLog:
//
// - LUNAR_INFO, LUNAR_WARN, LUNAR_ERROR, etc. — auto-capture __FILE__,
//   __LINE__, __func__ at the call site
// - LUNAR_ERROR_EX, LUNAR_FATAL_EX — exception variants with source location
// - LUNAR_LOG — generic macro accepting any LogLevel
// - setCaptureSourceLocation(true) — include source location in output
// - LUNAR_LOG_NO_MACROS — define before including lunar_log.hpp to opt out
//
// Compile: g++ -std=c++17 -I include examples/source_location_macros.cpp -o source_location_macros -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // Add structured sink to see source location fields
    logger.addSink<minta::FileSink>("macro_example.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_example.json.log");

    // ---------------------------------------------------------------
    // Basic usage: LUNAR_INFO, LUNAR_WARN, LUNAR_ERROR
    //
    // These macros automatically capture __FILE__, __LINE__, __func__
    // at the call site — no need for logWithSourceLocation().
    // ---------------------------------------------------------------
    std::cout << "=== Basic macro usage ===" << std::endl;

    LUNAR_INFO(logger, "Server started on port {port}", 8080);
    LUNAR_WARN(logger, "Cache miss rate at {rate}%", 15.3);
    LUNAR_ERROR(logger, "Failed to connect to {host}:{port}", "db-01", 5432);

    // ---------------------------------------------------------------
    // Exception variant: LUNAR_ERROR_EX
    //
    // Attach a caught std::exception to the log entry with automatic
    // source location capture.
    // ---------------------------------------------------------------
    std::cout << "=== Exception variant ===" << std::endl;

    try {
        throw std::runtime_error("connection refused: host=db-01 port=5432");
    } catch (const std::exception& ex) {
        LUNAR_ERROR_EX(logger, ex, "Database query failed for user {name}", "alice");
    }

    // ---------------------------------------------------------------
    // LUNAR_LOG generic macro
    //
    // Pass any LogLevel explicitly. Same level-gate and source
    // location capture as the convenience macros.
    // ---------------------------------------------------------------
    std::cout << "=== Generic LUNAR_LOG macro ===" << std::endl;

    LUNAR_LOG(logger, minta::LogLevel::DEBUG, "Debug via generic macro");
    LUNAR_LOG(logger, minta::LogLevel::FATAL, "Fatal via generic macro");

    // ---------------------------------------------------------------
    // setCaptureSourceLocation(true) — see location in output
    //
    // When enabled, the human-readable formatter includes file:line
    // and the JSON formatter includes file, line, function fields.
    // The macros always pass source location; this flag controls
    // whether the logger records it in the log entry.
    // ---------------------------------------------------------------
    std::cout << "=== Source location in output ===" << std::endl;

    logger.setCaptureSourceLocation(true);
    LUNAR_INFO(logger, "This entry includes source location in output");
    LUNAR_WARN(logger, "User {user} rate limited", "bob");
    logger.setCaptureSourceLocation(false);

    // ---------------------------------------------------------------
    // Level gating — arguments are NOT evaluated when level is
    // disabled, avoiding unnecessary work.
    // ---------------------------------------------------------------
    std::cout << "=== Level gating (no side effects) ===" << std::endl;

    logger.setMinLevel(minta::LogLevel::WARN);
    int counter = 0;
    LUNAR_TRACE(logger, "This is skipped, counter stays {c}", ++counter);
    std::cout << "Counter after disabled TRACE: " << counter
              << " (expected 0)" << std::endl;

    logger.setMinLevel(minta::LogLevel::TRACE);

    // ---------------------------------------------------------------
    // Note: LUNAR_LOG_NO_MACROS opt-out
    //
    // If you prefer not to have macros polluting your namespace,
    // define LUNAR_LOG_NO_MACROS before including lunar_log.hpp:
    //
    //   #define LUNAR_LOG_NO_MACROS
    //   #include "lunar_log.hpp"
    //   // LUNAR_INFO, LUNAR_LOG, etc. are NOT defined
    //   // logWithSourceLocation() and LUNAR_LOG_CONTEXT still work
    //
    // ---------------------------------------------------------------

    logger.flush();

    std::cout << "\nSource location macro examples complete."
              << "\nCheck macro_example.log and macro_example.json.log for output."
              << std::endl;

    return 0;
}
