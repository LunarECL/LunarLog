// batched_sink.cpp
//
// Demonstrates creating a custom batched sink that buffers entries and
// delivers them in batches. BatchedSink is an abstract base class — you
// extend it and implement writeBatch() to process batches (e.g., send
// over HTTP, write to a database, or append to a file in bulk).
//
// Compile: g++ -std=c++11 -I include examples/batched_sink.cpp -o batched_sink -pthread

#include "lunar_log.hpp"
#include <lunar_log/sink/batched_sink.hpp>
#include <iostream>
#include <mutex>

// ---------------------------------------------------------------
// Example: A simple batched console sink that prints batches
// ---------------------------------------------------------------
class BatchedConsoleSink : public minta::BatchedSink {
public:
    explicit BatchedConsoleSink(minta::BatchOptions opts = minta::BatchOptions())
        : minta::BatchedSink(opts)
    {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
    }

    ~BatchedConsoleSink() noexcept override {
        stopAndFlush();
    }

protected:
    void writeBatch(const std::vector<const minta::LogEntry*>& batch) override {
        std::lock_guard<std::mutex> lock(m_printMutex);
        std::cout << "=== Batch of " << batch.size() << " entries ===" << std::endl;
        for (size_t i = 0; i < batch.size(); ++i) {
            std::cout << "  [" << i << "] " << formatter()->format(*batch[i]) << std::endl;
        }
        std::cout << "=== End batch ===" << std::endl;
    }

    void onFlush() override {
        std::lock_guard<std::mutex> lock(m_printMutex);
        std::cout << "(batch flushed successfully)" << std::endl;
    }

    void onBatchError(const std::exception& e, size_t retryCount) override {
        std::lock_guard<std::mutex> lock(m_printMutex);
        std::cerr << "(batch error, retry " << retryCount << "): " << e.what() << std::endl;
    }

private:
    std::mutex m_printMutex;
};

int main() {
    // --- Example 1: Small batch size ---
    {
        minta::LunarLog logger(minta::LogLevel::TRACE, false);

        minta::BatchOptions opts;
        opts.setBatchSize(3)           // flush every 3 entries
            .setFlushIntervalMs(2000)  // or every 2 seconds
            .setMaxRetries(2);

        auto sink = minta::detail::make_unique<BatchedConsoleSink>(opts);
        logger.addCustomSink(std::move(sink));

        logger.info("First message");
        logger.info("Second message");
        logger.info("Third message — triggers batch flush");

        logger.warn("Fourth message");
        logger.error("Fifth message");
        // Destructor flushes remaining entries

        logger.flush();
    }

    std::cout << "\nBatched sink example completed." << std::endl;
    return 0;
}
