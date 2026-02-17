/// @file scoped_context.cpp
/// @brief Demonstrates RAII scoped context with LogScope.
///
/// Build:
///   g++ -std=c++17 -I../include -o scoped_context scoped_context.cpp
///
/// Expected output (timestamps will differ):
///   [... ] [INFO] Processing request [requestId=req-001, userId=u-42]
///   [... ] [INFO] Validating input [requestId=req-001, userId=u-42, step=validate]
///   [... ] [INFO] Saving to DB [requestId=req-001, userId=u-42, step=save]
///   [... ] [INFO] Request complete [requestId=req-001, userId=u-42]
///   [... ] [INFO] No context here

#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::INFO);

    // --- Basic scoped context ---
    {
        auto scope = logger.scope({{"requestId", "req-001"}, {"userId", "u-42"}});
        logger.info("Processing request");

        // Nested scope adds more context
        {
            auto inner = logger.scope({{"step", "validate"}});
            logger.info("Validating input");
        }
        // "step" removed

        {
            auto inner = logger.scope({{"step", "save"}});
            logger.info("Saving to DB");
        }

        logger.info("Request complete");
    }
    // All context removed
    logger.info("No context here");

    // --- Duplicate key shadowing ---
    std::cout << "\n--- Duplicate key shadowing ---\n";
    {
        auto outer = logger.scope({{"env", "production"}});
        logger.info("Outer: {env}", "env", "check context");

        {
            auto inner = logger.scope({{"env", "staging"}});
            logger.info("Inner shadows outer");
            // context: env=staging (inner wins)
        }

        logger.info("Outer restored");
        // context: env=production
    }

    // --- add() after construction ---
    std::cout << "\n--- Dynamic add ---\n";
    {
        auto scope = logger.scope({{"txn", "tx-001"}});
        scope.add("phase", "init").add("retry", "0");
        logger.info("Transaction started");
    }

    // --- Move semantics ---
    std::cout << "\n--- Move from function ---\n";
    auto makeScope = [&logger]() {
        return logger.scope({{"fromFactory", "yes"}});
    };

    {
        auto scope = makeScope();
        logger.info("Factory scope active");
    }
    logger.info("Factory scope gone");

    return 0;
}
