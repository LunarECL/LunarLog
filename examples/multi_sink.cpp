// multi_sink.cpp
//
// Demonstrates using multiple sinks with independent formatters, log levels,
// and locales. Each sink operates independently — a single log call can
// produce different output in each sink.
//
// Compile: g++ -std=c++17 -I include examples/multi_sink.cpp -o multi_sink -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Disable default console sink — we configure everything manually
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    // ---------------------------------------------------------------
    // Add sinks with different formatters
    //
    // Sink 0: Console — human-readable (for developers)
    // Sink 1: File    — human-readable (full log)
    // Sink 2: File    — JSON (for log aggregation)
    // Sink 3: File    — XML (for compliance/audit)
    // Sink 4: File    — human-readable (errors only)
    // ---------------------------------------------------------------
    logger.addSink<minta::ConsoleSink>();                                       // sink 0
    logger.addSink<minta::FileSink>("sink.log");                                // sink 1
    logger.addSink<minta::FileSink, minta::JsonFormatter>("sink.json.log");     // sink 2
    logger.addSink<minta::FileSink, minta::XmlFormatter>("sink.xml.log");       // sink 3
    logger.addSink<minta::FileSink>("sink.errors.log");                         // sink 4

    // ---------------------------------------------------------------
    // Per-sink log levels
    //
    // Sink 4 only receives ERROR and FATAL messages.
    // Other sinks use the default (TRACE — receives everything).
    // ---------------------------------------------------------------
    logger.setSinkLevel(4, minta::LogLevel::ERROR);

    // ---------------------------------------------------------------
    // Per-sink locales
    //
    // Sink 1 (full log) uses English formatting.
    // Sink 2 (JSON) uses German formatting.
    // Other sinks use the logger-level locale.
    // ---------------------------------------------------------------
    logger.setLocale("en_US");
    logger.setSinkLocale(2, "de_DE");

    // ---------------------------------------------------------------
    // Log at various levels — each sink filters independently
    // ---------------------------------------------------------------
    logger.trace("Entering function processOrder");
    // -> sinks 0-3 (not sink 4: below ERROR)

    logger.debug("Order ID: {id}, items: {count}", "ORD-123", 5);
    // -> sinks 0-3

    logger.info("Processing payment of {amount:n} for {customer}", 1234.56, "alice");
    // -> sinks 0-3
    // sink 0 (en_US): 1,234.56
    // sink 2 (de_DE): 1.234,56

    logger.warn("Inventory low for SKU {sku}: {remaining} left", "WIDGET-42", 3);
    // -> sinks 0-3

    logger.error("Payment gateway timeout for order {id}", "ORD-123");
    // -> ALL sinks including sink 4

    logger.fatal("Database connection lost");
    // -> ALL sinks including sink 4

    // ---------------------------------------------------------------
    // Per-sink filters combined with levels
    //
    // Add a filter to sink 3 (XML) to only log entries with context
    // ---------------------------------------------------------------
    logger.addSinkFilterRule(3, "context has 'audit'");

    logger.info("Regular log — not in XML (no audit context)");

    logger.setContext("audit", "true");
    logger.info("Audited action: user {user} deleted record {id}", "admin", "REC-789");
    // -> sink 3 (XML) only receives this one because of the audit filter
    logger.clearAllContext();

    logger.clearSinkFilterRules(3);

    // ---------------------------------------------------------------
    // Summary of sink configuration
    // ---------------------------------------------------------------
    logger.info("Multi-sink logging complete");

    // Flush to ensure all sinks finish writing
    logger.flush();

    std::cout << "\nMulti-sink examples complete."
              << "\nConsole output above (sink 0) — human-readable, all levels."
              << "\nCheck sink.log (sink 1) — human-readable, all levels."
              << "\nCheck sink.json.log (sink 2) — JSON, all levels, de_DE locale."
              << "\nCheck sink.xml.log (sink 3) — XML, all levels."
              << "\nCheck sink.errors.log (sink 4) — human-readable, ERROR+ only."
              << std::endl;
    return 0;
}
