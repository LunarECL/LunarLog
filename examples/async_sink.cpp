// async_sink.cpp
//
// Demonstrates wrapping a slow sink (FileSink) with AsyncSink to prevent
// blocking the logging thread. Each AsyncSink has its own queue and consumer
// thread for independent, non-blocking I/O.
//
// Compile: g++ -std=c++11 -I include examples/async_sink.cpp -o async_sink -pthread

#include "lunar_log.hpp"
#include <lunar_log/sink/async_sink.hpp>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // --- Example 1: Default async options ---
    {
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::TRACE)
            .writeTo<minta::AsyncSink<minta::FileSink>>("async_demo.log")
            .writeTo<minta::ConsoleSink>()
            .build();

        logger.info("Async logging started");
        logger.debug("This goes through the async queue to the file sink");
        logger.warn("Console sink receives directly, file sink via async queue");

        // flush() waits for the async queue to drain
        logger.flush();
    }

    // --- Example 2: Custom async options ---
    {
        minta::AsyncOptions opts;
        opts.queueSize = 4096;                          // smaller queue
        opts.overflowPolicy = minta::OverflowPolicy::DropOldest; // drop old under pressure
        opts.flushIntervalMs = 1000;                    // periodic flush every second

        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::TRACE)
            .writeTo<minta::AsyncSink<minta::FileSink>>(opts, std::string("async_custom.log"))
            .build();

        for (int i = 0; i < 100; ++i) {
            logger.info("High-throughput message {i}", "i", i);
        }

        logger.flush();
    }

    std::cout << "Async sink examples completed. Check async_demo.log and async_custom.log." << std::endl;
    return 0;
}
