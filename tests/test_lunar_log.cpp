#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void testBasicLogging() {
    minta::LunarLog logger(minta::LunarLog::Level::TRACE, "test_log.txt");

    logger.trace("This is a trace message");
    logger.debug("This is a debug message with a number: {number}", 42);
    logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
    logger.warn("Warning: {attempts} attempts remaining", 3);
    logger.error("Error occurred: {error}", "File not found");
    logger.fatal("Fatal error: {error}", "System crash");
}

void testJsonLogging() {
    try {
        minta::LunarLog logger(minta::LunarLog::Level::TRACE, "json_log.txt", 10 * 1024 * 1024, true);

        logger.trace("This is a trace message");
        logger.debug("This is a debug message with a number: {number}", 42);
        logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
        logger.warn("Warning: {attempts} attempts remaining", 3);
        logger.error("Error occurred: {error}", "File not found");
        logger.fatal("Fatal error: {error}", "System crash");

        // Uncomment to test validation:
        // logger.info("User {} logged in from {}", "alice", "192.168.1.1");
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
    }
}

void testLogLevels() {
    minta::LunarLog logger(minta::LunarLog::Level::WARN, "level_test_log.txt");

    logger.trace("This should not be logged");
    logger.debug("This should not be logged");
    logger.info("This should not be logged");
    logger.warn("This warning should be logged");
    logger.error("This error should be logged");
    logger.fatal("This fatal error should be logged");
}

void testRateLimiting() {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "rate_limit_test_log.txt");

    for (int i = 0; i < 1200; ++i) {
        logger.info("Message {}", i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This message should appear after the rate limit reset");
}

void testFileRotation() {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "rotation_test_log.txt", 100); // Small max file size for testing

    for (int i = 0; i < 100; ++i) {
        logger.info("This is a long message to test file rotation: {}", i);
    }
}

int main() {
    std::cout << "Testing basic logging..." << std::endl;
    testBasicLogging();

    std::cout << "Testing JSON logging..." << std::endl;
    testJsonLogging();

    std::cout << "Testing log levels..." << std::endl;
    testLogLevels();

    std::cout << "Testing rate limiting..." << std::endl;
    testRateLimiting();

    std::cout << "Testing file rotation..." << std::endl;
    testFileRotation();

    std::cout << "All tests completed. Please check the generated log files." << std::endl;

    return 0;
}
