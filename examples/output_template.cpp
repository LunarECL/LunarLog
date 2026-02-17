// output_template.cpp
//
// Demonstrates per-sink human-readable output templates via SinkProxy::outputTemplate().
//
// Includes examples for:
// - built-in tokens: {timestamp}, {level}, {message}, {template}, {properties},
//   {source}, {threadId}, {newline}
// - alignment: {level,6:u3}, {message,-20}
// - custom timestamp format: {timestamp:HH:mm:ss}

#include "lunar_log.hpp"

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // Keep default console sink as a JSON sink for contrast, then customize both.
    logger.setLocale("en_US");
    logger.setCaptureSourceLocation(true);

    logger.sink("sink_0").outputTemplate("[{timestamp:HH:mm:ss}] [{level,5:u3}] {message}");

    logger.addSink<minta::FileSink>("out.tsv");
    logger.sink("sink_1").outputTemplate("{timestamp} {level} {template} {newline}{properties}");

    logger.setContext("service", "payments");

    logger.info("User {user} logged in", "user", "alice");
    logger.info("Request completed in {ms}ms", "ms", 42);

    logger.clearContext("service");
    return 0;
}
