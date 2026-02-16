// context_capture.cpp
//
// Demonstrates context capture features in LunarLog:
//
// - setCaptureSourceLocation(true)  — include file, line, function in entries
// - setContext(key, value)          — add persistent key-value context
// - clearContext(key)               — remove a single context key
// - clearAllContext()               — remove all context keys
// - ContextScope                    — RAII automatic context cleanup
// - logWithSourceLocation()         — manual source location specification
//
// Compile: g++ -std=c++17 -I include examples/context_capture.cpp -o context_capture -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // Also log to JSON so we can see context in structured output
    logger.addSink<minta::FileSink, minta::JsonFormatter>("context.json.log");

    // ---------------------------------------------------------------
    // Source location capture
    //
    // When enabled, every log entry includes the file path, line number,
    // and function name from the call site.
    // ---------------------------------------------------------------
    logger.info("Without source location — no file/line shown");

    logger.setCaptureSourceLocation(true);
    logger.info("With source location — file and line included");

    // ---------------------------------------------------------------
    // Manual source location via logWithSourceLocation
    //
    // Use the LUNAR_LOG_CONTEXT macro which expands to
    // __FILE__, __LINE__, __func__
    // ---------------------------------------------------------------
    logger.logWithSourceLocation(
        minta::LogLevel::INFO,
        LUNAR_LOG_CONTEXT,
        "Explicit source location from {func}",
        "main"
    );

    // Turn off source location for cleaner output in remaining examples
    logger.setCaptureSourceLocation(false);

    // ---------------------------------------------------------------
    // Custom context: setContext / clearContext
    //
    // Context key-value pairs are attached to every subsequent log entry.
    // They appear in human-readable output as {key=value} and in
    // JSON output under the "context" object.
    // ---------------------------------------------------------------
    logger.setContext("session_id", "sess-abc123");
    logger.info("Login attempt for {user}", "alice");
    // Output includes: {session_id=sess-abc123}

    // Add another context key
    logger.setContext("request_id", "req-456");
    logger.info("Processing payment of {amount:C}", 29.99);
    // Output includes: {request_id=req-456, session_id=sess-abc123}

    // Remove one context key
    logger.clearContext("request_id");
    logger.info("Request complete");
    // Output includes: {session_id=sess-abc123} (request_id removed)

    // ---------------------------------------------------------------
    // ContextScope — RAII context management
    //
    // ContextScope sets a context key in its constructor and removes
    // it in its destructor. This ensures the key is cleaned up even
    // if an exception is thrown.
    //
    // The scope must not outlive the LunarLog instance.
    // ---------------------------------------------------------------
    {
        minta::ContextScope scope(logger, "operation", "checkout");
        logger.info("Starting checkout for {user}", "alice");
        // Output includes: {operation=checkout, session_id=sess-abc123}

        {
            // Scopes can be nested
            minta::ContextScope innerScope(logger, "step", "validate");
            logger.info("Validating cart contents");
            // Output includes: {operation=checkout, session_id=sess-abc123, step=validate}
        }
        // "step" is automatically removed here

        logger.info("Checkout complete");
        // Output includes: {operation=checkout, session_id=sess-abc123}
    }
    // "operation" is automatically removed here

    logger.info("Back to session context only");
    // Output includes: {session_id=sess-abc123}

    // ---------------------------------------------------------------
    // clearAllContext — remove everything at once
    // ---------------------------------------------------------------
    logger.clearAllContext();
    logger.info("All context cleared — no context in this entry");

    // ---------------------------------------------------------------
    // Context with filters — context keys can be used in DSL rules
    // ---------------------------------------------------------------
    logger.setContext("env", "production");
    logger.addFilterRule("context has 'env'");
    logger.info("Has env context — passes filter");

    logger.clearAllContext();
    logger.info("No env context — dropped by filter");

    logger.clearAllFilters();

    // Flush and report
    logger.flush();

    std::cout << "\nContext capture examples complete."
              << "\nCheck context.json.log for structured context output." << std::endl;
    return 0;
}
