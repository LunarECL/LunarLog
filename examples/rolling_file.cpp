/// @file rolling_file.cpp
/// @brief Demonstrates rolling file sink with size-based rotation.
///
/// Build:
///   g++ -std=c++17 -I../include -o rolling_file rolling_file.cpp

#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::INFO, false);

    // Size-based rotation: 1KB per file, keep 3 rolled files
    logger.addSink<minta::RollingFileSink>(
        minta::RollingPolicy::size("demo_roll.log", 1024).maxFiles(3));

    std::cout << "Writing 100 messages to demo_roll.log...\n";
    for (int i = 0; i < 100; ++i) {
        logger.info("Rolling file message {idx} with padding text", i);
    }
    logger.flush();

    std::cout << "Done! Check demo_roll.log and demo_roll.001.log etc.\n";
    return 0;
}
