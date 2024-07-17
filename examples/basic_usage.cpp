#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // Create a logger with minimum level set to TRACE
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // Add a console sink with default formatter
    logger.addSink<minta::ConsoleSink>();

    // Add a file sink with default formatter
    logger.addSink<minta::FileSink>("app.log");

    // Add a file sink with built-in JSON formatter
    logger.addSink<minta::FileSink, minta::JsonFormatter>("app.json.log");

    // Basic logging with named placeholders
    logger.trace("This is a trace message");
    logger.debug("This is a debug message with a number: {number}", 42);
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");
    logger.warn("Warning: {attempts} attempts remaining", 3);
    logger.error("Error occurred: {error}", "File not found");
    logger.fatal("Fatal error: {errorType}", "System crash");

    // Demonstrating escaped brackets
    logger.info("Escaped brackets example: {{escaped}} {notEscaped}", "value");

    // Demonstrate rate limiting
    for (int i = 0; i < 2000; ++i) {
        logger.info("Rate limit test message {index}", i);
    }

    // Wait for a second to reset rate limiting
    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");

    // Demonstrating placeholder validation warnings
    logger.info("Empty placeholder: {}", "value");
    logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");
    logger.info("Too few values: {placeholder1} and {placeholder2}", "value");
    logger.info("Too many values: {placeholder}", "value1", "value2");

    std::cout << "Check app.log and app.json.log for the logged messages." << std::endl;

    return 0;
}
