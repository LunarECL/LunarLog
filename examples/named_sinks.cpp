// named_sinks.cpp
//
// Demonstrates named sink registration, SinkProxy fluent API, auto-naming,
// sink lookup by name, and sink removal.
//
// Key concepts:
//   - named() / SinkName tag type for explicit sink naming
//   - Auto-naming: unnamed sinks get "sink_0", "sink_1", etc.
//   - SinkProxy: fluent API returned by logger.sink("name")
//   - Sink lookup and configuration by name instead of index
//
// Compile: g++ -std=c++17 -I include examples/named_sinks.cpp -o named_sinks -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Disable default console sink — we configure everything manually
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    // ---------------------------------------------------------------
    // 1. Named sink registration with named() factory
    //
    // Use named("name") to give a sink a human-readable name.
    // This is clearer than tracking sink indices manually.
    // ---------------------------------------------------------------
    logger.addSink<minta::ConsoleSink>(minta::named("console"));
    logger.addSink<minta::FileSink>(minta::named("app-log"), "app.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>(minta::named("json-out"), "app.json.log");

    // You can also use a plain string instead of named():
    logger.addSink<minta::FileSink>("errors", "errors.log");

    // ---------------------------------------------------------------
    // 2. SinkProxy fluent API
    //
    // logger.sink("name") returns a SinkProxy that lets you configure
    // the sink with a fluent (chainable) interface.
    // ---------------------------------------------------------------

    // Set the error sink to only receive ERROR+ messages
    logger.sink("errors").level(minta::LogLevel::ERROR);

    // Chain multiple configurations on the JSON sink
    logger.sink("json-out")
        .level(minta::LogLevel::INFO)
        .filterRule("not message contains 'heartbeat'");

    // Set a locale on a specific sink
    logger.sink("app-log").locale("en_US");

    // ---------------------------------------------------------------
    // 3. Logging — sinks filter independently
    // ---------------------------------------------------------------
    logger.trace("Detailed trace message");
    // -> console only (json-out is INFO+, errors is ERROR+)

    logger.info("User {name} logged in from {ip}", "alice", "192.168.1.1");
    // -> console, app-log, json-out (not errors — below ERROR)

    logger.info("heartbeat check");
    // -> console, app-log (json-out filters out "heartbeat")

    logger.error("Database connection failed: {reason}", "timeout");
    // -> ALL sinks including errors

    // ---------------------------------------------------------------
    // 4. Auto-naming behavior
    //
    // Sinks added without a name get auto-named "sink_0", "sink_1",
    // etc. Auto-names skip over any names already taken by user-named
    // sinks.
    // ---------------------------------------------------------------

    // (In this example all sinks are explicitly named, but if you
    // called logger.addSink<ConsoleSink>() without a name, it would
    // be auto-named "sink_0" or the next available index.)

    // ---------------------------------------------------------------
    // 5. Sink lookup — configure by name at any time
    //
    // You can look up and reconfigure sinks by name after creation.
    // This is especially useful for runtime configuration changes.
    // ---------------------------------------------------------------

    // Dynamically adjust the console sink level at runtime
    logger.sink("console").level(minta::LogLevel::WARN);

    logger.info("This INFO no longer appears on console");
    logger.warn("But this WARN does");

    // Reset console back to TRACE
    logger.sink("console").level(minta::LogLevel::TRACE);

    // ---------------------------------------------------------------
    // 6. Clearing filters on a named sink
    //
    // SinkProxy provides clearFilters() (predicate + DSL rules)
    // and clearTagFilters() (only/except tags).
    // ---------------------------------------------------------------
    logger.sink("json-out").clearFilters();
    logger.info("heartbeat check — now reaches json-out too");

    // Flush to ensure all output is written
    logger.flush();

    std::cout << "\nNamed sink examples complete."
              << "\nCheck app.log, app.json.log, and errors.log for results."
              << std::endl;
    return 0;
}
