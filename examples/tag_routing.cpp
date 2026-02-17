// tag_routing.cpp
//
// Demonstrates tag-based routing: [tag] prefix parsing, only/except
// filtering, multi-tag messages, and tags in JSON output.
//
// Key concepts:
//   - Prefix messages with [tag] to categorize them
//   - Use only("tag") on a sink to receive ONLY messages with that tag
//   - Use except("tag") on a sink to reject messages with that tag
//   - Tags appear in JSON and XML output automatically
//   - Tags combine with the existing filter pipeline
//
// Compile: g++ -std=c++17 -I include examples/tag_routing.cpp -o tag_routing -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Disable default console sink — we configure everything manually
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    // ---------------------------------------------------------------
    // 1. Setup: named sinks with different tag routing
    //
    // We create four sinks, each with different tag filters:
    //   - "console"  : receives everything (no tag filters)
    //   - "auth-log" : only receives messages tagged [auth]
    //   - "db-log"   : only receives messages tagged [db]
    //   - "main-log" : receives everything EXCEPT [health] messages
    // ---------------------------------------------------------------
    logger.addSink<minta::ConsoleSink>(minta::named("console"));
    logger.addSink<minta::FileSink>(minta::named("auth-log"), "auth.log");
    logger.addSink<minta::FileSink>(minta::named("db-log"), "db.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>(minta::named("main-log"), "main.json.log");

    // Configure tag routing via SinkProxy
    logger.sink("auth-log").only("auth");
    logger.sink("db-log").only("db");
    logger.sink("main-log").except("health");

    // ---------------------------------------------------------------
    // 2. Basic tag usage
    //
    // Prefix a message with [tagname] to tag it. The tag prefix is
    // stripped from the rendered message — it's metadata, not content.
    // ---------------------------------------------------------------
    logger.info("[auth] User {name} logged in", "alice");
    // -> console (no filter), auth-log (matches "auth"), main-log (not "health")
    // -> NOT db-log (only accepts "db")

    logger.info("[db] Query executed in {ms}ms", 42);
    // -> console, db-log (matches "db"), main-log
    // -> NOT auth-log (only accepts "auth")

    logger.info("[health] Heartbeat OK");
    // -> console (no filter), auth-log? NO (only "auth"), db-log? NO (only "db")
    // -> NOT main-log (excepts "health")
    // Result: console only

    logger.info("General message without tags");
    // -> console, main-log
    // -> NOT auth-log (has only() filter, untagged messages are excluded)
    // -> NOT db-log (same reason)

    // ---------------------------------------------------------------
    // 3. Multi-tag messages
    //
    // Multiple tags must be adjacent (no spaces between brackets):
    //   "[auth][security] message" — two tags: auth, security
    //   "[auth] [security] message" — only one tag: auth
    // ---------------------------------------------------------------
    logger.warn("[auth][security] Suspicious login attempt from {ip}", "10.0.0.99");
    // Tags: ["auth", "security"]
    // -> console, auth-log (matches "auth"), main-log
    // -> NOT db-log (neither tag is "db")

    // ---------------------------------------------------------------
    // 4. Tag validation
    //
    // Tags must contain only alphanumeric characters, hyphens, and
    // underscores. Invalid tags stop the parse — remaining text is
    // treated as the message.
    // ---------------------------------------------------------------
    logger.info("[valid-tag_123] This tag is valid");
    logger.info("[invalid tag] Space breaks tag parsing — treated as literal text");
    logger.info("No brackets at start — no tags parsed");

    // ---------------------------------------------------------------
    // 5. Tags in JSON output
    //
    // The JsonFormatter includes a "tags" array in the output.
    // The message field contains the tag-stripped text.
    // ---------------------------------------------------------------

    // This message goes to main-log (JSON formatter):
    logger.info("[db][perf] Slow query: {query} took {ms}ms", "SELECT *", 1500);
    // JSON output includes:
    //   "tags": ["db", "perf"],
    //   "message": "Slow query: SELECT * took 1500ms"

    // ---------------------------------------------------------------
    // 6. Combining tags with other filters
    //
    // Tag routing runs BEFORE per-sink level and DSL filters.
    // Pipeline: global filters -> tag routing -> sink level -> sink filters
    // ---------------------------------------------------------------

    // Add a level filter to the auth-log: only WARN+
    logger.sink("auth-log").level(minta::LogLevel::WARN);

    logger.info("[auth] Normal login by {name}", "bob");
    // -> auth-log has only("auth") match, but INFO < WARN — filtered out

    logger.warn("[auth] Failed login attempt for {name}", "admin");
    // -> auth-log: matches "auth" AND WARN >= WARN — kept

    // ---------------------------------------------------------------
    // 7. Multiple only() tags on one sink
    //
    // A sink with multiple only() tags accepts messages matching ANY
    // of them (OR logic).
    // ---------------------------------------------------------------
    logger.sink("db-log").only("cache"); // now accepts "db" OR "cache"

    logger.info("[cache] Cache miss for key {key}", "user:42");
    // -> db-log: matches "cache" — accepted

    logger.info("[db] Connection pool stats: {active}/{total}", 5, 10);
    // -> db-log: matches "db" — still accepted

    // ---------------------------------------------------------------
    // 8. Clearing tag filters
    // ---------------------------------------------------------------
    logger.sink("db-log").clearTagFilters();
    // db-log now receives ALL messages (no tag filtering)

    logger.info("Untagged message now reaches db-log too");

    // Flush to ensure all output is written
    logger.flush();

    std::cout << "\nTag routing examples complete."
              << "\nCheck auth.log, db.log, and main.json.log for results."
              << std::endl;
    return 0;
}
