// thread_safety.cpp
//
// Demonstrates thread-safe logging from multiple concurrent threads.
//
// LunarLog thread safety contract:
// - Add all sinks before logging (addSink after first log throws logic_error)
// - LunarLog must outlive all logging threads
// - setContext/clearContext/clearAllContext are thread-safe
// - setMinLevel/setCaptureSourceLocation are atomic
// - setFilter/addFilterRule and per-sink variants are thread-safe
// - setLocale/setSinkLocale are thread-safe
// - ContextScope must not outlive the LunarLog instance
//
// Compile: g++ -std=c++17 -I include examples/thread_safety.cpp -o thread_safety -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("threads.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("threads.json.log");

    // ---------------------------------------------------------------
    // 1. Multiple threads logging concurrently
    //
    // Each thread logs messages with a thread identifier. All messages
    // from all threads appear in the output (subject to rate limiting).
    // ---------------------------------------------------------------
    std::cout << "Starting concurrent logging from 8 threads..." << std::endl;

    const int numThreads = 8;
    const int messagesPerThread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; ++i) {
                logger.info("Thread {tid} message {seq}", t, i);
            }
        });
    }

    // Join all threads before continuing — required by the thread safety contract
    for (auto& th : threads) {
        th.join();
    }

    logger.flush();
    std::cout << "Phase 1 complete: " << (numThreads * messagesPerThread)
              << " messages logged from " << numThreads << " threads." << std::endl;

    // ---------------------------------------------------------------
    // 2. Concurrent context modification
    //
    // setContext and clearContext are thread-safe. Multiple threads
    // can update context concurrently while other threads log.
    // ---------------------------------------------------------------
    std::cout << "\nStarting concurrent context modification..." << std::endl;
    threads.clear();

    // Writer threads: update context
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < 20; ++i) {
                std::ostringstream oss;
                oss << "thread" << t << "_iter" << i;
                logger.setContext("worker", oss.str());
                logger.info("Context updated by thread {tid}", t);
            }
        });
    }

    // Reader threads: just log (they see whatever context is current)
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < 20; ++i) {
                logger.info("Reader thread {tid} iteration {seq}", t + 4, i);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    logger.clearAllContext();
    logger.flush();
    std::cout << "Phase 2 complete: concurrent context modification successful." << std::endl;

    // ---------------------------------------------------------------
    // 3. Concurrent filter modification
    //
    // Filter predicates, DSL rules, and per-sink filters can be
    // modified while other threads are logging.
    // ---------------------------------------------------------------
    std::cout << "\nStarting concurrent filter modification..." << std::endl;
    threads.clear();

    // Logger thread: logs continuously
    threads.emplace_back([&logger]() {
        for (int i = 0; i < 100; ++i) {
            logger.info("Continuous log message {seq}", i);
            logger.warn("Continuous warning {seq}", i);
        }
    });

    // Filter thread: modifies filters while logging happens
    threads.emplace_back([&logger]() {
        for (int i = 0; i < 10; ++i) {
            logger.addFilterRule("level >= WARN");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            logger.clearFilterRules();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    logger.clearAllFilters();
    logger.flush();
    std::cout << "Phase 3 complete: concurrent filter modification successful." << std::endl;

    // ---------------------------------------------------------------
    // 4. ContextScope across threads
    //
    // ContextScope is RAII — it sets context in its constructor and
    // clears it in its destructor. Each thread can have its own scope.
    // Note: since context is shared across the logger (not per-thread),
    // scopes from different threads will overwrite each other's keys.
    // Use unique keys per thread if isolation is needed.
    // ---------------------------------------------------------------
    std::cout << "\nStarting ContextScope in threads..." << std::endl;
    threads.clear();

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&logger, t]() {
            // Each thread uses a unique context key to avoid collision
            std::string key = "scope_t" + std::to_string(t);
            minta::ContextScope scope(logger, key, "active");
            for (int i = 0; i < 10; ++i) {
                logger.info("Thread {tid} with scoped context, msg {seq}", t, i);
            }
            // scope destructor clears the key when the thread exits
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    logger.flush();
    std::cout << "Phase 4 complete: ContextScope in threads successful." << std::endl;

    // ---------------------------------------------------------------
    // 5. Concurrent locale changes
    //
    // setLocale and setSinkLocale are thread-safe.
    // ---------------------------------------------------------------
    std::cout << "\nStarting concurrent locale changes..." << std::endl;
    threads.clear();

    threads.emplace_back([&logger]() {
        for (int i = 0; i < 20; ++i) {
            logger.info("Number: {val:n}", 1234567.89);
        }
    });

    threads.emplace_back([&logger]() {
        const char* locales[] = {"en_US", "de_DE", "fr_FR", "C"};
        for (int i = 0; i < 20; ++i) {
            logger.setLocale(locales[i % 4]);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    logger.setLocale("C");
    logger.flush();
    std::cout << "Phase 5 complete: concurrent locale changes successful." << std::endl;

    std::cout << "\nAll thread safety examples complete."
              << "\nCheck threads.log and threads.json.log for output." << std::endl;
    return 0;
}
