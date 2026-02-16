// custom_sink.cpp
//
// Demonstrates how to create a custom sink by extending minta::ISink.
//
// A sink receives formatted log entries and writes them to a destination.
// You can use the built-in formatter via formatter()->format(entry), or
// implement your own formatting logic directly in write().
//
// Compile: g++ -std=c++17 -I include examples/custom_sink.cpp -o custom_sink -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <mutex>

// ---------------------------------------------------------------
// Example 1: Simple custom sink that writes to stdout with a prefix
//
// Extends ISink directly, uses the built-in formatter, and writes
// the result to stdout with a custom prefix.
// ---------------------------------------------------------------
class PrefixedConsoleSink : public minta::ISink {
public:
    explicit PrefixedConsoleSink(const std::string& prefix) : m_prefix(prefix) {
        // Set a formatter in the constructor. This is required if you
        // want to use formatter()->format(entry) in write().
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
    }

    void write(const minta::LogEntry& entry) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << m_prefix << " " << formatter()->format(entry) << std::endl;
    }

private:
    std::string m_prefix;
    std::mutex m_mutex;
};

// ---------------------------------------------------------------
// Example 2: In-memory sink that collects log entries
//
// Useful for testing or buffering logs before sending them to
// an external service.
// ---------------------------------------------------------------
class MemorySink : public minta::ISink {
public:
    MemorySink() {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
    }

    void write(const minta::LogEntry& entry) override {
        std::string formatted = formatter()->format(entry);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back(formatted);
    }

    // Retrieve all collected entries
    std::vector<std::string> getEntries() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries;
    }

    size_t count() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

private:
    mutable std::mutex m_mutex;
    std::vector<std::string> m_entries;
};

// ---------------------------------------------------------------
// Example 3: Filtering sink with custom write logic
//
// Shows that you can implement arbitrary logic in write(), not just
// format-and-output. This sink counts entries by level.
// ---------------------------------------------------------------
class CountingSink : public minta::ISink {
public:
    CountingSink() {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
        for (int i = 0; i <= static_cast<int>(minta::LogLevel::FATAL); ++i) {
            m_counts[static_cast<minta::LogLevel>(i)] = 0;
        }
    }

    void write(const minta::LogEntry& entry) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counts[entry.level]++;
    }

    void printSummary() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << "\n--- Log Level Counts ---" << std::endl;
        for (const auto& pair : m_counts) {
            std::cout << minta::getLevelString(pair.first) << ": " << pair.second << std::endl;
        }
    }

private:
    mutable std::mutex m_mutex;
    std::map<minta::LogLevel, int> m_counts;
};

int main() {
    // Disable default console sink — we add our own custom sinks
    minta::LunarLog logger(minta::LogLevel::TRACE, false);

    // ---------------------------------------------------------------
    // Register custom sinks using addCustomSink()
    //
    // addCustomSink takes a std::unique_ptr<ISink>. The logger takes
    // ownership of the sink.
    // ---------------------------------------------------------------

    // Keep raw pointers for post-flush inspection (logger owns the objects)
    auto memorySinkPtr = new MemorySink();
    auto countingSinkPtr = new CountingSink();

    // Sink 0: prefixed console
    logger.addCustomSink(minta::detail::make_unique<PrefixedConsoleSink>("[APP]"));

    // Sink 1: in-memory buffer
    // Transfer ownership but keep a non-owning pointer for later inspection
    std::unique_ptr<minta::ISink> memorySink(memorySinkPtr);
    logger.addCustomSink(std::move(memorySink));

    // Sink 2: counting sink
    std::unique_ptr<minta::ISink> countingSink(countingSinkPtr);
    logger.addCustomSink(std::move(countingSink));

    // ---------------------------------------------------------------
    // Log some messages — all three custom sinks receive them
    // ---------------------------------------------------------------
    logger.trace("Application starting");
    logger.debug("Loading configuration from {path}", "/etc/app.conf");
    logger.info("Server listening on port {port}", 8080);
    logger.warn("Cache miss ratio above threshold: {ratio:P}", 0.35);
    logger.error("Failed to connect to database: {reason}", "timeout");
    logger.info("Request handled in {ms}ms", 42);

    // Flush to ensure all messages are processed
    logger.flush();

    // ---------------------------------------------------------------
    // Inspect the in-memory sink
    // ---------------------------------------------------------------
    std::cout << "\n--- Memory Sink: " << memorySinkPtr->count()
              << " entries collected ---" << std::endl;
    for (const auto& entry : memorySinkPtr->getEntries()) {
        std::cout << "  " << entry << std::endl;
    }

    // ---------------------------------------------------------------
    // Print level counts from the counting sink
    // ---------------------------------------------------------------
    countingSinkPtr->printSummary();

    return 0;
}
