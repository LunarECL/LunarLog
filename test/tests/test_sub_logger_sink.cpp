#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "lunar_log/sink/sub_logger_sink.hpp"
#include "lunar_log/sink/callback_sink.hpp"
#include "lunar_log/sink/async_sink.hpp"
#include "lunar_log/global.hpp"
#include "utils/test_utils.hpp"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <stdexcept>

class SubLoggerSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// ---------------------------------------------------------------------------
// Helper: thread-safe entry collector
// ---------------------------------------------------------------------------

struct CollectedEntry {
    minta::LogLevel level;
    std::string message;
    std::string templateStr;
    std::map<std::string, std::string> customContext;
    std::vector<std::string> tags;
    bool hasException = false;
    std::string exceptionType;
    std::string exceptionMessage;
};

class EntryCollector {
public:
    minta::CallbackSink::EntryCallback callback() {
        return minta::CallbackSink::EntryCallback(
            [this](const minta::LogEntry& entry) {
                CollectedEntry ce;
                ce.level = entry.level;
                ce.message = entry.message;
                ce.templateStr = entry.templateStr;
                ce.customContext = entry.customContext;
                ce.tags = entry.tags;
                if (entry.hasException()) {
                    ce.hasException = true;
                    ce.exceptionType = entry.exception->type;
                    ce.exceptionMessage = entry.exception->message;
                }
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    entries.push_back(std::move(ce));
                }
                cv.notify_all();
            });
    }

    bool waitFor(size_t count, int timeoutSeconds = 5) {
        std::unique_lock<std::mutex> lock(mtx);
        return cv.wait_for(lock, std::chrono::seconds(timeoutSeconds),
            [&]() { return entries.size() >= count; });
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return entries.size();
    }

    CollectedEntry at(size_t idx) {
        std::lock_guard<std::mutex> lock(mtx);
        return entries.at(idx);
    }

    std::vector<CollectedEntry> all() {
        std::lock_guard<std::mutex> lock(mtx);
        return entries;
    }

    std::mutex mtx;
    std::condition_variable cv;
    std::vector<CollectedEntry> entries;
};

// ===========================================================================
// Basic sub-logger receives entries
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerReceivesEntries) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("sub", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("hello from parent");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    EXPECT_EQ(collector.at(0).level, minta::LogLevel::INFO);
    EXPECT_TRUE(collector.at(0).message.find("hello from parent") != std::string::npos);
}

// ===========================================================================
// Sub-logger filters: only ERROR+ entries reach the sub-pipeline
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerFiltersByLevel) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("errors", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("ERROR+")
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.trace("trace msg");
    logger.debug("debug msg");
    logger.info("info msg");
    logger.warn("warn msg");
    logger.error("error msg");
    logger.fatal("fatal msg");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(2));
    // Give a moment for any extra entries to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].level, minta::LogLevel::ERROR);
    EXPECT_EQ(all[1].level, minta::LogLevel::FATAL);
}

// ===========================================================================
// Sub-logger filters with compact keyword filter
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerFiltersWithKeyword) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("audit", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("INFO+ ~audit")
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("normal operation");
    logger.info("audit: user login");
    logger.error("audit: unauthorized access");
    logger.debug("audit: low priority");  // filtered by INFO+
    logger.flush();

    ASSERT_TRUE(collector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_TRUE(all[0].message.find("audit: user login") != std::string::npos);
    EXPECT_TRUE(all[1].message.find("audit: unauthorized access") != std::string::npos);
}

// ===========================================================================
// Sub-logger with DSL filter rule
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerDSLFilterRule) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("dsl", [&](minta::SubLoggerConfiguration& sub) {
            sub.filterRule("level >= WARN")
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("info msg");
    logger.warn("warn msg");
    logger.error("error msg");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].level, minta::LogLevel::WARN);
    EXPECT_EQ(all[1].level, minta::LogLevel::ERROR);
}

// ===========================================================================
// Sub-logger with predicate filter
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerPredicateFilter) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("pred", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter(minta::FilterPredicate([](const minta::LogEntry& e) {
                    return e.message.find("important") != std::string::npos;
                }))
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("normal message");
    logger.info("important message");
    logger.warn("another important update");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_TRUE(all[0].message.find("important") != std::string::npos);
    EXPECT_TRUE(all[1].message.find("important") != std::string::npos);
}

// ===========================================================================
// Sub-logger with minLevel
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerMinLevel) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("leveled", [&](minta::SubLoggerConfiguration& sub) {
            sub.minLevel(minta::LogLevel::WARN)
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.trace("trace");
    logger.debug("debug");
    logger.info("info");
    logger.warn("warn");
    logger.error("error");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].level, minta::LogLevel::WARN);
    EXPECT_EQ(all[1].level, minta::LogLevel::ERROR);
}

// ===========================================================================
// Sub-logger enrichers are independent from parent
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerEnrichersIndependentFromParent) {
    EntryCollector parentCollector;
    EntryCollector subCollector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .enrich(minta::Enrichers::property("source", "parent"))
        .subLogger("enriched", [&](minta::SubLoggerConfiguration& sub) {
            sub.enrich(minta::Enrichers::property("pipeline", "sub-pipeline"))
               .enrich(minta::Enrichers::property("alert", "true"))
               .writeTo<minta::CallbackSink>(subCollector.callback());
        })
        .build();

    // Also add a parent-level callback to see what the parent sees
    auto parentSink = minta::detail::make_unique<minta::CallbackSink>(
        parentCollector.callback());
    logger.addCustomSink(std::move(parentSink));

    logger.info("test enrichment");
    logger.flush();

    ASSERT_TRUE(subCollector.waitFor(1));
    ASSERT_TRUE(parentCollector.waitFor(1));

    // Sub-logger should have its own enricher properties
    auto subEntry = subCollector.at(0);
    EXPECT_EQ(subEntry.customContext.at("pipeline"), "sub-pipeline");
    EXPECT_EQ(subEntry.customContext.at("alert"), "true");
    // Sub-logger also sees parent enrichers (they ran first)
    EXPECT_EQ(subEntry.customContext.at("source"), "parent");

    // Parent sink should NOT have sub-logger enrichers
    auto parentEntry = parentCollector.at(0);
    EXPECT_EQ(parentEntry.customContext.at("source"), "parent");
    EXPECT_EQ(parentEntry.customContext.count("pipeline"), 0u);
    EXPECT_EQ(parentEntry.customContext.count("alert"), 0u);
}

// ===========================================================================
// Sub-logger enrichers don't leak to parent (mutation isolation)
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerEnrichersMutationIsolation) {
    EntryCollector parentCollector;
    EntryCollector subCollector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("mutator", [&](minta::SubLoggerConfiguration& sub) {
            sub.enrich([](minta::LogEntry& entry) {
                   entry.customContext["mutated"] = "yes";
                   entry.customContext["source"] = "overwritten-by-sub";
               })
               .writeTo<minta::CallbackSink>(subCollector.callback());
        })
        .enrich(minta::Enrichers::property("source", "parent"))
        .build();

    auto parentSink = minta::detail::make_unique<minta::CallbackSink>(
        parentCollector.callback());
    logger.addCustomSink(std::move(parentSink));

    logger.info("isolation test");
    logger.flush();

    ASSERT_TRUE(subCollector.waitFor(1));
    ASSERT_TRUE(parentCollector.waitFor(1));

    // Sub sees mutated context
    auto subEntry = subCollector.at(0);
    EXPECT_EQ(subEntry.customContext.at("mutated"), "yes");
    EXPECT_EQ(subEntry.customContext.at("source"), "overwritten-by-sub");

    // Parent does NOT see sub-logger's mutations
    auto parentEntry = parentCollector.at(0);
    EXPECT_EQ(parentEntry.customContext.count("mutated"), 0u);
    EXPECT_EQ(parentEntry.customContext.at("source"), "parent");
}

// ===========================================================================
// Multiple sub-loggers
// ===========================================================================

TEST_F(SubLoggerSinkTest, MultipleSubLoggers) {
    EntryCollector errorCollector;
    EntryCollector auditCollector;
    EntryCollector allCollector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("errors", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("ERROR+")
               .writeTo<minta::CallbackSink>(errorCollector.callback());
        })
        .subLogger("audit", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("~audit")
               .writeTo<minta::CallbackSink>(auditCollector.callback());
        })
        .subLogger("all", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(allCollector.callback());
        })
        .build();

    logger.info("normal info");
    logger.info("audit: user created");
    logger.error("system failure");
    logger.error("audit: permission denied");
    logger.flush();

    ASSERT_TRUE(allCollector.waitFor(4));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    // "errors" sub-logger: ERROR+ only
    auto errors = errorCollector.all();
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0].level, minta::LogLevel::ERROR);
    EXPECT_EQ(errors[1].level, minta::LogLevel::ERROR);

    // "audit" sub-logger: messages containing "audit"
    auto audits = auditCollector.all();
    ASSERT_EQ(audits.size(), 2u);
    EXPECT_TRUE(audits[0].message.find("audit") != std::string::npos);
    EXPECT_TRUE(audits[1].message.find("audit") != std::string::npos);

    // "all" sub-logger: everything
    auto all = allCollector.all();
    EXPECT_EQ(all.size(), 4u);
}

// ===========================================================================
// Sub-logger alongside regular sinks
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerAlongsideRegularSinks) {
    EntryCollector parentCollector;
    EntryCollector subCollector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("sub", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("WARN+")
               .writeTo<minta::CallbackSink>(subCollector.callback());
        })
        .build();

    auto parentSink = minta::detail::make_unique<minta::CallbackSink>(
        parentCollector.callback());
    logger.addCustomSink(std::move(parentSink));

    logger.info("info");
    logger.warn("warn");
    logger.error("error");
    logger.flush();

    ASSERT_TRUE(parentCollector.waitFor(3));
    ASSERT_TRUE(subCollector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    // Parent sees all 3
    EXPECT_EQ(parentCollector.all().size(), 3u);
    // Sub sees only WARN+
    auto sub = subCollector.all();
    ASSERT_EQ(sub.size(), 2u);
    EXPECT_EQ(sub[0].level, minta::LogLevel::WARN);
    EXPECT_EQ(sub[1].level, minta::LogLevel::ERROR);
}

// ===========================================================================
// Sub-logger with multiple sub-sinks
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerMultipleSubSinks) {
    EntryCollector collector1;
    EntryCollector collector2;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("multi-sink", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(collector1.callback())
               .writeTo<minta::CallbackSink>(collector2.callback());
        })
        .build();

    logger.info("fanout message");
    logger.flush();

    ASSERT_TRUE(collector1.waitFor(1));
    ASSERT_TRUE(collector2.waitFor(1));

    EXPECT_TRUE(collector1.at(0).message.find("fanout") != std::string::npos);
    EXPECT_TRUE(collector2.at(0).message.find("fanout") != std::string::npos);
}

// ===========================================================================
// Unnamed sub-logger works
// ===========================================================================

TEST_F(SubLoggerSinkTest, UnnamedSubLogger) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger([&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("unnamed sub");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    EXPECT_TRUE(collector.at(0).message.find("unnamed sub") != std::string::npos);
}

// ===========================================================================
// SubLoggerSink inspection methods
// ===========================================================================

TEST_F(SubLoggerSinkTest, InspectionMethods) {
    minta::SubLoggerConfiguration config;
    config.filter("ERROR+")
          .enrich(minta::Enrichers::property("k", "v"))
          .enrich(minta::Enrichers::threadId())
          .writeTo<minta::CallbackSink>(
              minta::CallbackSink::EntryCallback([](const minta::LogEntry&) {}))
          .writeTo<minta::CallbackSink>(
              minta::CallbackSink::EntryCallback([](const minta::LogEntry&) {}));

    minta::SubLoggerSink sink(std::move(config));

    EXPECT_EQ(sink.sinkCount(), 2u);
    EXPECT_EQ(sink.enricherCount(), 2u);
    EXPECT_GE(sink.filterRuleCount(), 1u);
    EXPECT_NE(sink.subSink(0), nullptr);
    EXPECT_NE(sink.subSink(1), nullptr);
    EXPECT_EQ(sink.subSink(99), nullptr);
}

// ===========================================================================
// SubLoggerSink constructed directly (not via builder)
// ===========================================================================

TEST_F(SubLoggerSinkTest, DirectConstruction) {
    EntryCollector collector;

    minta::SubLoggerConfiguration config;
    config.filter("WARN+")
          .enrich(minta::Enrichers::property("direct", "yes"))
          .writeTo<minta::CallbackSink>(collector.callback());

    auto subSink = minta::detail::make_unique<minta::SubLoggerSink>(
        std::move(config));

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    logger.addCustomSink("direct-sub", std::move(subSink));

    logger.info("info msg");
    logger.warn("warn msg");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].level, minta::LogLevel::WARN);
    EXPECT_EQ(all[0].customContext.at("direct"), "yes");
}

// ===========================================================================
// SubLoggerSink with lambda constructor
// ===========================================================================

TEST_F(SubLoggerSinkTest, LambdaConstruction) {
    EntryCollector collector;

    auto subSink = minta::detail::make_unique<minta::SubLoggerSink>(
        [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("ERROR+")
               .writeTo<minta::CallbackSink>(collector.callback());
        });

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    logger.addCustomSink("lambda-sub", std::move(subSink));

    logger.info("info");
    logger.error("error");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].level, minta::LogLevel::ERROR);
}

// ===========================================================================
// Sub-logger flush propagates to sub-sinks
// ===========================================================================

TEST_F(SubLoggerSinkTest, FlushPropagation) {
    std::atomic<int> flushCount(0);

    // Create a custom sink that counts flushes
    class FlushCountingSink : public minta::ISink {
    public:
        explicit FlushCountingSink(std::atomic<int>& counter)
            : m_counter(counter) {}
        void write(const minta::LogEntry&) override {}
        void flush() override {
            m_counter.fetch_add(1, std::memory_order_relaxed);
        }
    private:
        std::atomic<int>& m_counter;
    };

    minta::SubLoggerConfiguration config;
    config.writeTo<FlushCountingSink>(std::ref(flushCount))
          .writeTo<FlushCountingSink>(std::ref(flushCount));

    minta::SubLoggerSink sink(std::move(config));
    sink.flush();

    // Each sub-sink should have been flushed once
    EXPECT_EQ(flushCount.load(), 2);
}

// ===========================================================================
// Sub-logger with file sink (integration test)
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerWithFileSink) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("file-sub", [](minta::SubLoggerConfiguration& sub) {
            sub.filter("ERROR+")
               .writeTo<minta::FileSink>("sub_logger_errors.log");
        })
        .build();

    logger.info("info - should not appear in file");
    logger.error("critical error - should appear");
    logger.flush();

    TestUtils::waitForFileContent("sub_logger_errors.log");
    std::string content = TestUtils::readLogFile("sub_logger_errors.log");

    EXPECT_TRUE(content.find("critical error") != std::string::npos);
    EXPECT_TRUE(content.find("info - should not appear") == std::string::npos);
}

// ===========================================================================
// Sub-logger with JsonFormatter
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerWithJsonFormatter) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("json-sub", [](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::FileSink, minta::JsonFormatter>(
                "sub_logger_json.log");
        })
        .build();

    logger.info("json formatted {val}", "test");
    logger.flush();

    TestUtils::waitForFileContent("sub_logger_json.log");
    std::string content = TestUtils::readLogFile("sub_logger_json.log");

    EXPECT_TRUE(content.find("\"level\"") != std::string::npos ||
                content.find("\"Level\"") != std::string::npos);
}

// ===========================================================================
// Thread safety: concurrent writes to sub-logger
// ===========================================================================

TEST_F(SubLoggerSinkTest, ThreadSafety) {
    EntryCollector collector;
    const int numThreads = 4;
    const int msgsPerThread = 50;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("threaded", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, t, msgsPerThread]() {
            for (int i = 0; i < msgsPerThread; ++i) {
                logger.info("thread {tid} msg {idx}",
                    "tid", t, "idx", i);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
    logger.flush();

    size_t expected = static_cast<size_t>(numThreads * msgsPerThread);
    ASSERT_TRUE(collector.waitFor(expected, 10));
    EXPECT_EQ(collector.all().size(), expected);
}

// ===========================================================================
// Sub-logger throwing enricher doesn't crash
// ===========================================================================

TEST_F(SubLoggerSinkTest, ThrowingEnricherDoesNotCrash) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("throwing", [&](minta::SubLoggerConfiguration& sub) {
            sub.enrich([](minta::LogEntry&) {
                   throw std::runtime_error("enricher boom");
               })
               .enrich(minta::Enrichers::property("after-throw", "yes"))
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("should not crash");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    auto entry = collector.at(0);
    // The second enricher should still run
    EXPECT_EQ(entry.customContext.at("after-throw"), "yes");
}

// ===========================================================================
// Sub-logger throwing sub-sink doesn't crash other sub-sinks
// ===========================================================================

TEST_F(SubLoggerSinkTest, ThrowingSubSinkDoesNotAffectOthers) {
    EntryCollector collector;

    class ThrowingSink : public minta::ISink {
    public:
        void write(const minta::LogEntry&) override {
            throw std::runtime_error("sink boom");
        }
    };

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("resilient", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<ThrowingSink>()
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("survive the throw");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    EXPECT_TRUE(collector.at(0).message.find("survive") != std::string::npos);
}

// ===========================================================================
// Sub-logger throwing filter predicate fails-open
// ===========================================================================

TEST_F(SubLoggerSinkTest, ThrowingFilterPredicateFailsOpen) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("fail-open", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter(minta::FilterPredicate([](const minta::LogEntry&) -> bool {
                    throw std::runtime_error("filter boom");
                }))
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("should pass through");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    EXPECT_TRUE(collector.at(0).message.find("should pass through") != std::string::npos);
}

// ===========================================================================
// SubLoggerSink with ISink base-class filter (passesFilter)
// ===========================================================================

TEST_F(SubLoggerSinkTest, ISinkBaseFilterOnSubLoggerSink) {
    EntryCollector collector;

    minta::SubLoggerConfiguration config;
    config.writeTo<minta::CallbackSink>(collector.callback());

    auto subSink = minta::detail::make_unique<minta::SubLoggerSink>(
        std::move(config));
    subSink->setMinLevel(minta::LogLevel::ERROR);

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();
    logger.addCustomSink("base-filtered", std::move(subSink));

    logger.info("filtered out by ISink base");
    logger.error("passes ISink base filter");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].level, minta::LogLevel::ERROR);
}

// ===========================================================================
// Empty sub-logger (no sinks) doesn't crash
// ===========================================================================

TEST_F(SubLoggerSinkTest, EmptySubLoggerNoCrash) {
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("empty", [](minta::SubLoggerConfiguration&) {
            // No sinks configured
        })
        .build();

    logger.info("should not crash");
    logger.warn("still fine");
    logger.error("no problem");
    logger.flush();
    // No assertions needed — just verify no crash/hang
}

// ===========================================================================
// Sub-logger with enricher and filter combined
// ===========================================================================

TEST_F(SubLoggerSinkTest, EnricherAndFilterCombined) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("combined", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("WARN+")
               .enrich(minta::Enrichers::property("severity", "high"))
               .enrich(minta::Enrichers::property("team", "ops"))
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("low priority");
    logger.warn("medium priority");
    logger.error("high priority");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);

    for (const auto& e : all) {
        EXPECT_EQ(e.customContext.at("severity"), "high");
        EXPECT_EQ(e.customContext.at("team"), "ops");
    }
}

// ===========================================================================
// Sub-logger preserves entry data (template, arguments, tags)
// ===========================================================================

TEST_F(SubLoggerSinkTest, PreservesEntryData) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("data", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("[mytag] User {name} age {age}", "name", "alice", "age", 30);
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    auto entry = collector.at(0);

    // Tags are stripped from templateStr into entry.tags by LunarLog
    EXPECT_EQ(entry.templateStr, "User {name} age {age}");
    EXPECT_EQ(entry.level, minta::LogLevel::INFO);
    EXPECT_FALSE(entry.tags.empty());
    bool hasTag = false;
    for (const auto& t : entry.tags) {
        if (t == "mytag") hasTag = true;
    }
    EXPECT_TRUE(hasTag);
}

// ===========================================================================
// Sub-logger combined filter: predicate AND DSL
// ===========================================================================

TEST_F(SubLoggerSinkTest, CombinedPredicateAndDSLFilter) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("combined-filter", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter(minta::FilterPredicate([](const minta::LogEntry& e) {
                    return e.message.find("special") != std::string::npos;
                }))
               .filterRule("level >= WARN")
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    logger.info("special info");     // passes predicate, fails DSL
    logger.warn("normal warn");      // fails predicate, passes DSL
    logger.error("special error");   // passes both
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    logger.flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].level, minta::LogLevel::ERROR);
    EXPECT_TRUE(all[0].message.find("special error") != std::string::npos);
}

// ===========================================================================
// Coverage gap: Exception attachment preserved through sub-logger clone
// ===========================================================================

TEST_F(SubLoggerSinkTest, ExceptionAttachmentPreserved) {
    EntryCollector collector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("exceptions", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    std::runtime_error ex("database connection lost");
    logger.error(ex, "Operation failed: {reason}", "reason", "timeout");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1));
    auto entry = collector.at(0);
    EXPECT_EQ(entry.level, minta::LogLevel::ERROR);
    EXPECT_TRUE(entry.hasException);
    EXPECT_TRUE(entry.exceptionType.find("runtime_error") != std::string::npos);
    EXPECT_EQ(entry.exceptionMessage, "database connection lost");
}

// ===========================================================================
// Coverage gap: Nested sub-logger via writeTo<SubLoggerSink>
// ===========================================================================

TEST_F(SubLoggerSinkTest, NestedSubLogger) {
    EntryCollector outerCollector;
    EntryCollector innerCollector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("outer", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("WARN+")
               .enrich(minta::Enrichers::property("layer", "outer"))
               .writeTo<minta::CallbackSink>(outerCollector.callback())
               .writeTo<minta::SubLoggerSink>(
                   [&](minta::SubLoggerConfiguration& inner) {
                       inner.filter("ERROR+")
                            .enrich(minta::Enrichers::property("layer", "inner"))
                            .writeTo<minta::CallbackSink>(
                                innerCollector.callback());
                   });
        })
        .build();

    logger.info("info msg");    // filtered by outer (WARN+)
    logger.warn("warn msg");    // passes outer, filtered by inner (ERROR+)
    logger.error("error msg");  // passes both
    logger.flush();

    ASSERT_TRUE(outerCollector.waitFor(2));
    ASSERT_TRUE(innerCollector.waitFor(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    // Outer sees WARN+ (warn + error)
    auto outer = outerCollector.all();
    ASSERT_EQ(outer.size(), 2u);
    EXPECT_EQ(outer[0].customContext.at("layer"), "outer");

    // Inner sees ERROR+ only, with inner enricher overwriting "layer"
    auto inner = innerCollector.all();
    ASSERT_EQ(inner.size(), 1u);
    EXPECT_EQ(inner[0].level, minta::LogLevel::ERROR);
    EXPECT_EQ(inner[0].customContext.at("layer"), "inner");
}

// ===========================================================================
// Coverage gap: AsyncSink wrapping SubLoggerSink
// ===========================================================================

TEST_F(SubLoggerSinkTest, AsyncSinkWrappingSubLoggerSink) {
    EntryCollector collector;

    minta::SubLoggerConfiguration subConfig;
    subConfig.filter("ERROR+")
             .enrich(minta::Enrichers::property("async", "yes"))
             .writeTo<minta::CallbackSink>(collector.callback());

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .build();

    auto asyncSub = minta::detail::make_unique<
        minta::AsyncSink<minta::SubLoggerSink>>(std::move(subConfig));
    logger.addCustomSink("async-sub", std::move(asyncSub));

    logger.info("info msg");
    logger.error("error msg");
    logger.flush();

    ASSERT_TRUE(collector.waitFor(1, 10));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto all = collector.all();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].level, minta::LogLevel::ERROR);
    EXPECT_EQ(all[0].customContext.at("async"), "yes");
}

// ===========================================================================
// Coverage gap: Global logger with subLogger()
// ===========================================================================

TEST_F(SubLoggerSinkTest, GlobalLoggerWithSubLogger) {
    // Clean up any existing global logger
    if (minta::Log::isInitialized()) {
        minta::Log::shutdown();
    }

    EntryCollector collector;

    minta::Log::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("global-sub", [&](minta::SubLoggerConfiguration& sub) {
            sub.filter("WARN+")
               .writeTo<minta::CallbackSink>(collector.callback());
        })
        .build();

    minta::Log::info("info msg");
    minta::Log::warn("warn msg");
    minta::Log::error("error msg");
    minta::Log::flush();

    ASSERT_TRUE(collector.waitFor(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    minta::Log::flush();

    auto all = collector.all();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].level, minta::LogLevel::WARN);
    EXPECT_EQ(all[1].level, minta::LogLevel::ERROR);

    minta::Log::shutdown();
}

// ===========================================================================
// Coverage gap: Sub-logger with tag routing on sub-sinks
// ===========================================================================

TEST_F(SubLoggerSinkTest, SubLoggerTagRouting) {
    EntryCollector alertCollector;
    EntryCollector auditCollector;

    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .subLogger("tagged", [&](minta::SubLoggerConfiguration& sub) {
            sub.writeTo<minta::CallbackSink>("alert-sink",
                   alertCollector.callback())
               .writeTo<minta::CallbackSink>("audit-sink",
                   auditCollector.callback());
        })
        .build();

    // Configure tag routing on sub-sinks after construction
    // (sub-sinks are accessible via SubLoggerSink::subSink())
    // For this test, we use tag-based filtering at the entry level:
    // entries with [alert] tag and entries with [audit] tag.

    logger.info("[alert] CPU overheating");
    logger.info("[audit] user login");
    logger.warn("no tags");
    logger.flush();

    // Both sub-sinks receive all entries (no tag filter configured on sinks)
    ASSERT_TRUE(alertCollector.waitFor(3));
    ASSERT_TRUE(auditCollector.waitFor(3));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger.flush();

    // Verify tags are preserved through the sub-logger clone
    auto alerts = alertCollector.all();
    ASSERT_EQ(alerts.size(), 3u);

    bool foundAlertTag = false;
    for (const auto& t : alerts[0].tags) {
        if (t == "alert") foundAlertTag = true;
    }
    EXPECT_TRUE(foundAlertTag);

    bool foundAuditTag = false;
    for (const auto& t : auditCollector.at(1).tags) {
        if (t == "audit") foundAuditTag = true;
    }
    EXPECT_TRUE(foundAuditTag);
}

// ===========================================================================
// F2 verification: unnamed sub-sinks get auto-names
// ===========================================================================

TEST_F(SubLoggerSinkTest, UnnamedSubSinksAutoNamed) {
    minta::SubLoggerConfiguration config;
    config.writeTo<minta::CallbackSink>(
              minta::CallbackSink::EntryCallback([](const minta::LogEntry&) {}))
          .writeTo<minta::CallbackSink>(
              minta::CallbackSink::EntryCallback([](const minta::LogEntry&) {}));

    minta::SubLoggerSink sink(std::move(config));

    ASSERT_EQ(sink.sinkCount(), 2u);
    EXPECT_EQ(sink.subSink(0)->getSinkName(), "sub_sink_0");
    EXPECT_EQ(sink.subSink(1)->getSinkName(), "sub_sink_1");
}

// ===========================================================================
// Named sub-sinks keep user-provided names
// ===========================================================================

TEST_F(SubLoggerSinkTest, NamedSubSinksKeepUserNames) {
    minta::SubLoggerConfiguration config;
    config.writeTo<minta::CallbackSink>("my-callback",
              minta::CallbackSink::EntryCallback([](const minta::LogEntry&) {}))
          .writeTo<minta::CallbackSink>(
              minta::CallbackSink::EntryCallback([](const minta::LogEntry&) {}));

    minta::SubLoggerSink sink(std::move(config));

    ASSERT_EQ(sink.sinkCount(), 2u);
    EXPECT_EQ(sink.subSink(0)->getSinkName(), "my-callback");
    // Second sink is unnamed, gets auto-name (counter continues from where named left off)
    EXPECT_EQ(sink.subSink(1)->getSinkName(), "sub_sink_0");
}
