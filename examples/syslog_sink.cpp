// syslog_sink.cpp
//
// Demonstrates sending log entries to the POSIX syslog daemon.
// SyslogSink maps LunarLog log levels to syslog priorities.
// Available on Linux, macOS, and BSD only.
//
// Compile: g++ -std=c++11 -I include examples/syslog_sink.cpp -o syslog_sink -pthread
// View:    tail -f /var/log/syslog   (Linux)
//          log stream --predicate 'processIdentifier == <PID>'  (macOS)

#ifndef _WIN32

#include "lunar_log.hpp"
#include <lunar_log/sink/syslog_sink.hpp>
#include <iostream>

int main() {
    // --- Example 1: Default syslog options ---
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);
        logger.addSink<minta::SyslogSink>("my-app");

        logger.info("Application started");
        logger.warn("Disk usage at {pct}%", "pct", 85);
        logger.error("Connection refused to {host}", "host", "db-primary");
    }

    // --- Example 2: Custom facility and level prefix ---
    {
        minta::SyslogOptions opts;
        opts.setFacility(LOG_LOCAL0)
            .setIncludeLevel(true);  // Prefix: "[INFO] message"

        minta::LunarLog logger(minta::LogLevel::DEBUG, false);
        logger.addSink<minta::SyslogSink>("my-daemon", opts);

        logger.debug("Debug with facility LOG_LOCAL0");
        logger.info("Info message with level prefix");
        logger.fatal("Critical failure — shutting down");
    }

    std::cout << "Syslog sink examples completed. Check your system log." << std::endl;
    return 0;
}

#else
#include <iostream>
int main() {
    std::cout << "SyslogSink is not available on Windows." << std::endl;
    return 0;
}
#endif
