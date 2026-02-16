// basic_usage.cpp
//
// Quick-start example covering core LunarLog features: log levels, sinks,
// formatters, context, source location, escaped brackets, and rate limiting.
//
// The constructor adds a ConsoleSink by default; pass false as the second
// argument to suppress it.
//
// Compile: g++ -std=c++17 -I include examples/basic_usage.cpp -o basic_usage -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // TRACE level + default ConsoleSink (sink 0)
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // File sinks with different formatters (sinks 1-3)
    logger.addSink<minta::FileSink>("app.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("app.json.log");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("app.xml.log");

    // All six log levels
    logger.trace("This is a trace message");
    logger.debug("This is a debug message with a number: {number}", 42);
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
    logger.warn("Warning: {attempts} attempts remaining", 3);
    logger.error("Error occurred: {error}", "File not found");
    logger.fatal("Fatal error: {errorType}", "System crash");

    // Format specifiers
    logger.info("Price: {amount:.2f}, Hex: {val:X}, Pct: {rate:P}", 3.14159, 255, 0.856);

    // Source location capture (file, line, function in output)
    logger.setCaptureSourceLocation(true);

    // Custom context key-value pairs
    logger.setContext("session_id", "abc123");
    logger.info("Log with custom context");

    // ContextScope — RAII cleanup of context keys
    {
        minta::ContextScope scope(logger, "request_id", "req456");
        logger.info("Log within context scope");
    }
    logger.info("Log after context scope");

    logger.clearAllContext();

    // Manual source location via LUNAR_LOG_CONTEXT macro
    logger.logWithSourceLocation(minta::LogLevel::INFO, LUNAR_LOG_CONTEXT, "Manual context specification");

    // Escaped brackets: {{ and }} produce literal braces
    logger.info("Escaped brackets example: {{escaped}} {notEscaped}", "value");

    // Rate limiting: 1000 msg/sec — excess messages are silently dropped
    for (int i = 0; i < 2000; ++i) {
        logger.info("Rate limit test message {index}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    logger.info("This message should appear after the rate limit reset");

    // Placeholder validation warnings (emitted as WARN entries)
    logger.info("Empty placeholder: {}", "value");
    logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");
    logger.info("Too few values: {placeholder1} and {placeholder2}", "value");
    logger.info("Too many values: {placeholder}", "value1", "value2");

    // Flush ensures all queued messages are written before exit
    logger.flush();

    std::cout << "Check app.log, app.json.log, and app.xml.log for the logged messages." << std::endl;

    return 0;
}