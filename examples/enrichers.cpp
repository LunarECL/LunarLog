#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    logger.addSink<minta::ConsoleSink>();
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enriched.json.log");

    logger.enrich(minta::Enrichers::threadId());
    logger.enrich(minta::Enrichers::processId());
    logger.enrich(minta::Enrichers::machineName());
    logger.enrich(minta::Enrichers::environment());
    logger.enrich(minta::Enrichers::property("version", "2.1.0"));
    logger.enrich(minta::Enrichers::caller());

    logger.setCaptureSourceLocation(true);

    std::cout << "=== Built-in enrichers ===" << std::endl;
    logger.info("Application started");
    logger.info("User {name} logged in", "alice");

    std::cout << "\n=== Custom lambda enricher ===" << std::endl;
    {
        minta::LunarLog logger2(minta::LogLevel::INFO, false);
        logger2.addSink<minta::ConsoleSink>();
        logger2.enrich([](minta::LogEntry& entry) {
            entry.customContext["correlationId"] = "corr-12345";
        });
        logger2.enrich(minta::Enrichers::property("service", "auth-api"));

        logger2.info("Processing request");
        logger2.info("Request complete");
        logger2.flush();
    }

    std::cout << "\n=== Precedence: setContext wins over enricher ===" << std::endl;
    {
        minta::LunarLog logger3(minta::LogLevel::INFO, false);
        logger3.addSink<minta::ConsoleSink>();
        logger3.enrich(minta::Enrichers::property("env", "from-enricher"));
        logger3.setContext("env", "from-setContext");
        logger3.info("env should be from-setContext");
        logger3.clearAllContext();
        logger3.flush();
    }

    std::cout << "\n=== Precedence: scoped context wins over enricher ===" << std::endl;
    {
        minta::LunarLog logger4(minta::LogLevel::INFO, false);
        logger4.addSink<minta::ConsoleSink>();
        logger4.enrich(minta::Enrichers::property("env", "from-enricher"));
        {
            auto scope = logger4.scope({{"env", "from-scope"}});
            logger4.info("env should be from-scope");
        }
        logger4.info("env should be from-enricher (no scope)");
        logger4.flush();
    }

    std::cout << "\n=== fromEnv enricher ===" << std::endl;
    {
        minta::LunarLog logger5(minta::LogLevel::INFO, false);
        logger5.addSink<minta::ConsoleSink>();
        logger5.enrich(minta::Enrichers::fromEnv("HOME", "homeDir"));
        logger5.info("Home directory attached");
        logger5.flush();
    }

    logger.flush();

    std::cout << "\nCheck enriched.json.log for structured output." << std::endl;

    return 0;
}
