// http_sink.cpp
//
// Demonstrates sending log batches to an HTTP endpoint using HttpSink.
// HttpSink extends BatchedSink and sends JSONL (newline-delimited JSON)
// payloads via HTTP POST. Uses raw sockets on POSIX, WinHTTP on Windows.
//
// Compile: g++ -std=c++11 -I include examples/http_sink.cpp -o http_sink -pthread
//
// To test, run a local HTTP server:
//   python3 -m http.server 8080
// or use a request-logging tool like https://requestbin.com

#include "lunar_log.hpp"
#include <lunar_log/sink/http_sink.hpp>
#include <iostream>

int main() {
    // --- Example 1: Basic HTTP sink ---
    {
        minta::HttpSinkOptions opts("http://localhost:8080/api/logs");
        opts.setBatchSize(5)
            .setFlushIntervalMs(3000)
            .setMaxRetries(2);

        minta::LunarLog logger(minta::LogLevel::INFO, false);

        // Add console sink for local visibility
        logger.addSink<minta::ConsoleSink>();
        logger.addSink<minta::HttpSink>(opts);

        logger.info("Application started");
        logger.warn("Disk usage at {pct}%", "pct", 85);
        logger.error("Connection refused to {host}", "host", "db-primary");

        logger.flush();
    }

    // --- Example 2: With custom headers (e.g., API key auth) ---
    {
        minta::HttpSinkOptions opts("http://localhost:8080/v2/logs");
        opts.setHeader("Authorization", "Bearer my-api-key")
            .setHeader("X-Service-Name", "my-app")
            .setContentType("application/x-ndjson")
            .setBatchSize(10)
            .setFlushIntervalMs(5000);

        minta::LunarLog logger(minta::LogLevel::DEBUG, false);
        logger.addSink<minta::HttpSink>(opts);

        for (int i = 0; i < 12; ++i) {
            logger.info("Event {n}", "n", i);
        }

        logger.flush();
    }

    std::cout << "HTTP sink examples completed." << std::endl;
    return 0;
}
