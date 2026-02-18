#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Full builder chain
    auto log = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .captureSourceLocation(true)
        .rateLimit(1000, std::chrono::seconds(1))
        .templateCacheSize(256)
        .enrich(minta::Enrichers::threadId())
        .enrich(minta::Enrichers::property("service", "payment-api"))
        .filter("WARN+")
        .writeTo<minta::ConsoleSink>("console")
        .writeTo<minta::FileSink>("app", "app.log")
        .writeTo<minta::FileSink>("errors", [](minta::SinkProxy& s) {
            s.level(minta::LogLevel::ERROR);
            s.formatter(minta::detail::make_unique<minta::JsonFormatter>());
        }, "errors.log")
        .build();

    log.info("Application started");
    log.warn("High memory usage: {pct}%", 87);
    log.error("Connection failed to {host}", "db-01");
    log.flush();

    std::cout << "\n--- Minimal builder ---\n" << std::endl;

    // Minimal builder
    {
        auto minimal = minta::LunarLog::configure()
            .writeTo<minta::ConsoleSink>()
            .build();

        minimal.info("Hello from minimal builder");
        minimal.flush();
    }

    std::cout << "\n--- Existing API unchanged ---\n" << std::endl;

    // Existing imperative API — unchanged
    {
        minta::LunarLog imperative(minta::LogLevel::DEBUG, false);
        imperative.addSink<minta::ConsoleSink>();
        imperative.setMinLevel(minta::LogLevel::DEBUG);
        imperative.info("Hello from imperative API");
        imperative.flush();
    }

    std::cout << "\nFluent builder examples complete." << std::endl;
    std::cout << "Check app.log and errors.log for output." << std::endl;

    return 0;
}
