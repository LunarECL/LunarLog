// color_console.cpp
//
// Demonstrates ColorConsoleSink — ANSI-colored log output.
// Each log level gets a distinct color for the [LEVEL] bracket.
//
// Run in a terminal (not piped) to see colors. Set NO_COLOR (any value)
// or LUNAR_LOG_NO_COLOR=1 to disable color output.
//
// Compile: g++ -std=c++11 -I include examples/color_console.cpp -o color_console -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Using the fluent builder with ColorConsoleSink
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .writeTo<minta::ColorConsoleSink>("color-console")
        .build();

    // All six levels — each with a different color
    logger.trace("This is a trace message (dim)");
    logger.debug("This is a debug message (cyan)");
    logger.info("User {name} logged in from {ip}", "name", "alice", "ip", "10.0.0.1");
    logger.warn("Disk usage at {pct}%", "pct", 87);
    logger.error("Connection to {host} failed", "host", "db-01");
    logger.fatal("Unrecoverable error: {reason}", "reason", "out of memory");

    logger.flush();

    std::cout << "\n--- Color can be toggled at runtime ---\n" << std::endl;

    // Imperative API
    minta::LunarLog logger2(minta::LogLevel::TRACE, false);
    logger2.addSink<minta::ColorConsoleSink>();
    logger2.info("Colors enabled by default");
    logger2.flush();

    std::cout << "Done. If running in a terminal, you should see colored [LEVEL] brackets above." << std::endl;
    return 0;
}
