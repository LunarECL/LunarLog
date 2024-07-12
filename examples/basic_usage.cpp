// usage.cpp
#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void logInThread(minta::LunarLog& logger, int threadId) {
    for (int i = 0; i < 5; ++i) {
        logger.info("Log from thread {threadId}, iteration {iteration}", threadId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    // Create a logger with minimum level set to TRACE
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // Add a console sink using the factory
    auto consoleSink = logger.addSink<minta::ConsoleSink>();

    // Add a file sink with rotation at 1MB using the factory
    auto fileSink = logger.addSink<minta::FileSink>("app.log", 1024 * 1024);

    // Add a JSON file sink using the factory
    auto jsonFileSink = logger.addSink<minta::FileSink>("app.json.log", 1024 * 1024);

    // Basic logging
    logger.trace("This is a trace message");
    logger.debug("This is a debug message");
    logger.info("This is an info message");
    logger.warn("This is a warning message");
    logger.error("This is an error message");
    logger.fatal("This is a fatal message");

    // Logging with placeholders
    int userId = 1234;
    std::string username = "alice";
    logger.info("User {userId} logged in with username {username}", userId, username);

    // Demonstrating escaped brackets
    logger.info("Escaped brackets example: {{escaped}} {not_escaped}", "value");

    // JSON logging
    logger.enableJsonLogging(true);
    logger.info("This message will be logged in JSON format");
    logger.info("User {username} performed action {action}", "bob", "delete");
    logger.enableJsonLogging(false);

    // Demonstrate rate limiting
    for (int i = 0; i < 2000; ++i) {
        logger.info("Rate limit test message {iteration}", i);
    }

    // Demonstrate multi-threaded logging
    std::thread t1(logInThread, std::ref(logger), 1);
    std::thread t2(logInThread, std::ref(logger), 2);
    t1.join();
    t2.join();

    logger.removeSink(consoleSink);

    logger.info("This message should appear in files but not in console");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.info("This is another message that should be in the file");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    logger.removeSinkOfType<minta::FileSink>();

    logger.info("This message should not appear anywhere as all sinks have been removed");

    std::cout << "Check app.log and app.json.log for the logged messages." << std::endl;

    std::ifstream logFile("app.log");
    if (logFile.is_open()) {
        std::cout << "\nContents of app.log:\n";
        std::cout << logFile.rdbuf();
    }
    return 0;
}