// structured_output.cpp
//
// Demonstrates structured logging output with JSON and XML formatters.
//
// Structured formatters include:
// - messageTemplate: the raw template string with placeholders
// - templateHash: FNV-1a hash (8-char hex) for efficient log grouping
// - properties: named values extracted from placeholders
// - context: custom key-value pairs from setContext()
//
// Also demonstrates the template cache configuration.
//
// Compile: g++ -std=c++17 -I include examples/structured_output.cpp -o structured_output -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // No default console sink — we control output format explicitly
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    // Add structured output sinks
    logger.addSink<minta::FileSink, minta::JsonFormatter>("structured.json.log");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("structured.xml.log");

    // Also add a console sink for human-readable output
    logger.addSink<minta::ConsoleSink>();

    // ---------------------------------------------------------------
    // Basic structured output
    //
    // JSON output includes messageTemplate and templateHash alongside
    // the rendered message. This enables log aggregation tools (Seq,
    // Elasticsearch, Splunk) to group entries by template.
    // ---------------------------------------------------------------
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
    // JSON:
    // {
    //   "level": "INFO",
    //   "timestamp": "...",
    //   "message": "User alice logged in from 192.168.1.1",
    //   "messageTemplate": "User {username} logged in from {ip}",
    //   "templateHash": "a1b2c3d4",
    //   "properties": {"username": "alice", "ip": "192.168.1.1"}
    // }

    // Same template, different values — same templateHash
    logger.info("User {username} logged in from {ip}", "bob", "10.0.0.5");
    // templateHash is identical, enabling grouping

    // Different template — different hash
    logger.info("User {username} logged out", "alice");

    // ---------------------------------------------------------------
    // Properties with operators
    //
    // @ (destructure) makes JSON emit native types for numeric/boolean values.
    // $ (stringify) forces JSON to emit string values.
    // ---------------------------------------------------------------
    logger.info("Order {@id} total: {$amount}", 1042, 59.99);
    // JSON properties: {"id": 1042, "amount": "59.99"}

    logger.info("Active: {@flag}, Count: {@num}", true, 7);
    // JSON properties: {"flag": true, "num": 7}

    // ---------------------------------------------------------------
    // Context in structured output
    //
    // Custom context appears in its own "context" object in JSON
    // and as a <context> element in XML.
    // ---------------------------------------------------------------
    logger.setContext("service", "auth-api");
    logger.setContext("version", "2.1.0");
    logger.info("Health check passed");
    // JSON: includes "context": {"service": "auth-api", "version": "2.1.0"}

    // Source location in structured output
    logger.setCaptureSourceLocation(true);
    logger.info("Request processed in {ms}ms", 23);
    // JSON: includes "file", "line", "function" fields
    logger.setCaptureSourceLocation(false);
    logger.clearAllContext();

    // ---------------------------------------------------------------
    // Escaped brackets in templates
    //
    // Double braces {{ and }} produce literal braces in the output.
    // They do not appear in the messageTemplate (template stores the
    // raw form with double braces).
    // ---------------------------------------------------------------
    logger.info("Config: {{env}} = {value}", "production");
    // message: "Config: {env} = production"
    // messageTemplate: "Config: {{env}} = {value}"

    // ---------------------------------------------------------------
    // XML output structure
    //
    // XML wraps each entry in <log_entry> with child elements for
    // level, timestamp, message, MessageTemplate (with hash attribute),
    // properties, and context.
    // ---------------------------------------------------------------
    logger.warn("Disk usage at {pct:P} on {drive}", 0.92, "/dev/sda1");
    // XML:
    // <log_entry>
    //   <level>WARN</level>
    //   <timestamp>...</timestamp>
    //   <message>Disk usage at 92.00% on /dev/sda1</message>
    //   <MessageTemplate hash="...">Disk usage at {pct:P} on {drive}</MessageTemplate>
    //   <properties><pct>0.92</pct><drive>/dev/sda1</drive></properties>
    // </log_entry>

    // ---------------------------------------------------------------
    // Template cache configuration
    //
    // Parsed templates are cached to avoid re-parsing on repeated log
    // calls. The default cache holds 128 entries. Once full, new
    // templates are parsed on every call but existing entries stay cached.
    // ---------------------------------------------------------------

    // Increase cache size for high-cardinality logging
    logger.setTemplateCacheSize(256);

    // Log with many different templates — up to 256 are cached
    for (int i = 0; i < 10; ++i) {
        logger.info("Request {id} status: {code}", i, 200);
    }
    // Same template used 10 times — parsed once, cached, reused 9 times

    // Disable caching (for testing or to save memory)
    logger.setTemplateCacheSize(0);
    logger.info("Template parsed fresh — caching disabled");

    // Re-enable caching
    logger.setTemplateCacheSize(128);

    // Flush and report
    logger.flush();

    std::cout << "\nStructured output examples complete."
              << "\nCheck structured.json.log for JSON output."
              << "\nCheck structured.xml.log for XML output." << std::endl;
    return 0;
}
