/// @file compact_filter.cpp
/// @brief Demonstrates compact filter syntax.
///
/// Build:
///   g++ -std=c++17 -I../include -o compact_filter compact_filter.cpp

#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::ConsoleSink>();

    // --- Level filter ---
    std::cout << "--- WARN+ filter ---\n";
    logger.filter("WARN+");
    logger.info("This is filtered out");
    logger.warn("This passes");
    logger.error("This also passes");
    logger.clearFilterRules();

    // --- Keyword filter ---
    std::cout << "\n--- ~timeout filter ---\n";
    logger.filter("~timeout");
    logger.info("Connection timeout detected");
    logger.info("Normal operation");  // filtered
    logger.clearFilterRules();

    // --- Negated keyword ---
    std::cout << "\n--- !~heartbeat filter ---\n";
    logger.filter("!~heartbeat");
    logger.info("Heartbeat OK");       // filtered
    logger.info("User logged in");     // passes
    logger.clearFilterRules();

    // --- Context filter ---
    std::cout << "\n--- ctx:env=prod filter ---\n";
    logger.filter("ctx:env=prod");
    logger.setContext("env", "prod");
    logger.info("Production log");     // passes
    logger.clearContext("env");
    logger.setContext("env", "dev");
    logger.info("Dev log");            // filtered
    logger.clearAllContext();
    logger.clearFilterRules();

    // --- Combined ---
    std::cout << "\n--- Combined: WARN+ !~heartbeat ---\n";
    logger.filter("WARN+ !~heartbeat");
    logger.info("Info message");              // filtered (level)
    logger.warn("Heartbeat warning");         // filtered (keyword)
    logger.warn("Real warning");              // passes both
    logger.error("Error occurred");           // passes both
    logger.clearFilterRules();

    return 0;
}
