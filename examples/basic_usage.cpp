#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    logger.addSink<minta::FileSink>("app.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("app.json.log");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("app.xml.log");

    // Basic usage without context capture
    logger.trace("This is a trace message");
    logger.debug("This is a debug message with a number: {number}", 42);
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
    logger.warn("Warning: {attempts} attempts remaining", 3);
    logger.error("Error occurred: {error}", "File not found");
    logger.fatal("Fatal error: {errorType}", "System crash");

    // Enable context capture
    logger.setCaptureContext(true);

    // Custom context
    logger.setContext("session_id", "abc123");
    logger.info("Log with custom context");

    // Scoped context
    {
        minta::ContextScope scope(logger, "request_id", "req456");
        logger.info("Log within context scope");
    }
    logger.info("Log after context scope");

    logger.clearAllContext();

    // Advanced usage with manual context specification
    logger.logWithContext(minta::LogLevel::INFO, LUNAR_LOG_CONTEXT, "Manual context specification");

    // Demonstrating escaped brackets
    logger.info("Escaped brackets example: {{escaped}} {notEscaped}", "value");

    // Rate limiting demonstration
    for (int i = 0; i < 2000; ++i) {
        logger.info("Rate limit test message {index}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");

    // Placeholder validation warnings
    logger.info("Empty placeholder: {}", "value");
    logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");
    logger.info("Too few values: {placeholder1} and {placeholder2}", "value");
    logger.info("Too many values: {placeholder}", "value1", "value2");

    std::cout << "Check app.log, app.json.log, and app.xml.log for the logged messages." << std::endl;

    return 0;
}