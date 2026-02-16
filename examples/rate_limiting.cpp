// rate_limiting.cpp
//
// Demonstrates the built-in rate limiting behavior in LunarLog.
//
// LunarLog enforces a hard limit of 1000 messages per second per logger
// instance. Messages exceeding the limit within a 1-second window are
// silently dropped. The window resets automatically after 1 second of
// wall-clock time.
//
// Validation warnings (from placeholder mismatches) are exempt from
// rate limiting.
//
// Compile: g++ -std=c++17 -I include examples/rate_limiting.cpp -o rate_limiting -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // Log to file to count messages easily
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("rate_limit.log");
    logger.addSink<minta::ConsoleSink>();

    // ---------------------------------------------------------------
    // Phase 1: Send messages within the rate limit
    //
    // 100 messages should all get through (well under 1000/sec).
    // ---------------------------------------------------------------
    logger.info("--- Phase 1: Under rate limit (100 messages) ---");
    for (int i = 0; i < 100; ++i) {
        logger.info("Phase 1 message {index}", i);
    }
    logger.flush();
    logger.info("Phase 1 complete: all 100 messages should appear");

    // ---------------------------------------------------------------
    // Phase 2: Exceed the rate limit
    //
    // Send 2000 messages rapidly. Only the first ~1000 will be accepted;
    // the rest are silently dropped. There is no warning or error —
    // the messages simply do not appear in the output.
    // ---------------------------------------------------------------
    logger.info("--- Phase 2: Exceeding rate limit (2000 messages) ---");
    int phase2Count = 2000;
    for (int i = 0; i < phase2Count; ++i) {
        logger.info("Phase 2 message {index}", i);
    }
    logger.flush();
    logger.info("Phase 2 complete: only ~1000 of 2000 messages should appear");

    // ---------------------------------------------------------------
    // Phase 3: Wait for the rate limit window to reset
    //
    // After sleeping 1 second, the rate limit window resets and new
    // messages are accepted again.
    // ---------------------------------------------------------------
    logger.info("--- Phase 3: Waiting for rate limit reset ---");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("Rate limit window has reset");
    for (int i = 0; i < 10; ++i) {
        logger.info("Phase 3 message {index}", i);
    }
    logger.flush();
    logger.info("Phase 3 complete: all 10 messages should appear");

    // ---------------------------------------------------------------
    // Notes
    //
    // - The rate limit is best-effort under concurrent access. At
    //   window boundaries, a small number of messages may be allowed
    //   beyond the limit or silently lost.
    // - The rate limit is per-logger, not per-sink. If you have multiple
    //   sinks, the 1000 msg/sec limit applies to all of them collectively.
    // - Validation warnings are exempt from rate limiting.
    // ---------------------------------------------------------------

    logger.flush();

    std::cout << "\nRate limiting example complete."
              << "\nCheck rate_limit.log and count lines to verify the limit."
              << std::endl;
    return 0;
}
