// filtering.cpp
//
// Demonstrates the multi-layer filtering system in LunarLog.
//
// Filter pipeline (evaluated in order):
//   global min level -> global predicate -> global DSL rules
//   -> per-sink min level -> per-sink predicate -> per-sink DSL rules
//
// Compile: g++ -std=c++17 -I include examples/filtering.cpp -o filtering -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Start with TRACE to let the filter system do the work
    minta::LunarLog logger(minta::LogLevel::TRACE, false);  // no default console sink

    // Add two file sinks so we can demonstrate independent filtering
    logger.addSink<minta::FileSink>("all.log");       // sink 0
    logger.addSink<minta::FileSink>("errors.log");     // sink 1
    logger.addSink<minta::FileSink>("filtered.log");   // sink 2

    // ---------------------------------------------------------------
    // 1. Per-sink log levels
    //
    // Each sink has its own minimum log level. The effective level is
    // the stricter of the global logger level and the per-sink level.
    // ---------------------------------------------------------------
    logger.setSinkLevel(1, minta::LogLevel::ERROR);  // sink 1: errors only

    logger.info("This INFO goes to sink 0 and 2, but NOT sink 1");
    logger.error("This ERROR goes to all three sinks");

    // ---------------------------------------------------------------
    // 2. Global predicate filter
    //
    // A function that receives each LogEntry and returns true to keep
    // the entry or false to drop it — applied before any sink sees it.
    // If the predicate throws, the entry is let through (fail-open).
    // ---------------------------------------------------------------
    logger.setFilter([](const minta::LogEntry& entry) {
        // Only keep WARN+ or entries with "important" context
        return entry.level >= minta::LogLevel::WARN ||
               entry.customContext.count("important") > 0;
    });

    logger.info("Dropped by global predicate (INFO, no 'important' context)");
    logger.warn("Kept by global predicate (WARN)");

    // Mark a message as important via context
    logger.setContext("important", "yes");
    logger.info("Kept because context has 'important' key");
    logger.clearContext("important");

    // Remove global predicate
    logger.clearFilter();

    // ---------------------------------------------------------------
    // 3. Per-sink predicate filter
    //
    // Same as global predicate but applied to a single sink.
    // ---------------------------------------------------------------
    logger.setSinkFilter(2, [](const minta::LogEntry& entry) {
        // Sink 2: suppress messages containing "sensitive"
        return entry.message.find("sensitive") == std::string::npos;
    });

    logger.info("Contains sensitive data — dropped from sink 2 only");
    logger.info("Normal message — reaches all sinks");

    // Remove per-sink predicate
    logger.clearSinkFilter(2);

    // ---------------------------------------------------------------
    // 4. Global DSL filter rules
    //
    // Declarative rules parsed from strings. Multiple rules are
    // AND-combined — all must pass for the entry to proceed.
    // ---------------------------------------------------------------
    logger.addFilterRule("level >= WARN");
    logger.addFilterRule("not message contains 'heartbeat'");

    logger.debug("Dropped: below WARN");
    logger.warn("Kept: WARN and no heartbeat");
    logger.warn("Dropped: heartbeat check at WARN");

    // Remove global DSL rules
    logger.clearFilterRules();

    // ---------------------------------------------------------------
    // 5. Per-sink DSL filter rules
    //
    // DSL rules applied to individual sinks.
    // ---------------------------------------------------------------
    logger.setContext("env", "production");

    logger.addSinkFilterRule(0, "context env == 'production'");
    logger.info("Reaches sink 0 (env=production matches rule)");

    logger.clearContext("env");
    logger.setContext("env", "staging");
    logger.info("Dropped from sink 0 (env=staging fails rule), reaches sinks 1/2");

    logger.clearSinkFilterRules(0);
    logger.clearAllContext();

    // ---------------------------------------------------------------
    // 6. DSL rule types — demonstration of each condition
    // ---------------------------------------------------------------

    // level == LEVEL
    logger.addFilterRule("level == ERROR");
    logger.warn("Dropped: not exactly ERROR");
    logger.error("Kept: exactly ERROR");
    logger.clearFilterRules();

    // level != LEVEL
    logger.addFilterRule("level != TRACE");
    logger.trace("Dropped: is TRACE");
    logger.debug("Kept: not TRACE");
    logger.clearFilterRules();

    // message contains 'text'
    logger.addFilterRule("message contains 'alert'");
    logger.info("Dropped: no 'alert' in message");
    logger.info("System alert triggered");
    logger.clearFilterRules();

    // message startswith 'text'
    logger.addFilterRule("message startswith 'ALERT'");
    logger.info("ALERT: disk full");
    logger.info("No alert here");
    logger.clearFilterRules();

    // template == 'exact'
    logger.addFilterRule("template == 'Request from {ip}'");
    logger.info("Request from {ip}", "10.0.0.1");
    logger.info("Other template {val}", "test");
    logger.clearFilterRules();

    // template contains 'partial'
    logger.addFilterRule("template contains 'logged in'");
    logger.info("User {name} logged in", "alice");
    logger.info("User {name} logged out", "alice");
    logger.clearFilterRules();

    // context has 'key'
    logger.addFilterRule("context has 'request_id'");
    logger.info("No request_id context — dropped");
    logger.setContext("request_id", "req-789");
    logger.info("Has request_id context — kept");
    logger.clearAllContext();
    logger.clearFilterRules();

    // not <rule> (negation)
    logger.addFilterRule("not message contains 'noisy'");
    logger.info("This is noisy debug output — dropped");
    logger.info("Quiet important message — kept");
    logger.clearFilterRules();

    // ---------------------------------------------------------------
    // 7. clearAllFilters — remove all global filters at once
    // ---------------------------------------------------------------
    logger.setFilter([](const minta::LogEntry&) { return false; });
    logger.addFilterRule("level >= FATAL");
    // Everything is blocked now
    logger.error("Blocked by both predicate and rule");
    // Clear everything
    logger.clearAllFilters();
    logger.info("All global filters cleared — this message gets through");

    // ---------------------------------------------------------------
    // 8. clearAllSinkFilters — remove all filters from a specific sink
    // ---------------------------------------------------------------
    logger.setSinkLevel(2, minta::LogLevel::FATAL);
    logger.setSinkFilter(2, [](const minta::LogEntry&) { return false; });
    logger.addSinkFilterRule(2, "level >= FATAL");
    // Sink 2 blocks everything
    logger.error("Blocked from sink 2");
    // Clear all sink 2 filters at once
    logger.clearAllSinkFilters(2);
    logger.info("Sink 2 filters cleared — this reaches sink 2 again");

    // Flush to ensure all output is written
    logger.flush();

    std::cout << "Filtering examples complete."
              << "\nCheck all.log, errors.log, and filtered.log for results." << std::endl;
    return 0;
}
