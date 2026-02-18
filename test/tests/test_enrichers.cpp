#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <cstdlib>

class EnricherTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- threadId enricher attaches threadId key ---

TEST_F(EnricherTest, ThreadIdEnricherAttachesKey) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_threadid.txt");
    logger.enrich(minta::Enrichers::threadId());

    logger.info("Thread ID test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_threadid.txt");
    std::string content = TestUtils::readLogFile("enricher_threadid.txt");

    EXPECT_TRUE(content.find("Thread ID test") != std::string::npos);
    EXPECT_TRUE(content.find("\"threadId\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"threadId\":\"\"") == std::string::npos);
}

// --- processId enricher attaches processId key (non-empty) ---

TEST_F(EnricherTest, ProcessIdEnricherAttachesNonEmptyKey) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_processid.txt");
    logger.enrich(minta::Enrichers::processId());

    logger.info("Process ID test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_processid.txt");
    std::string content = TestUtils::readLogFile("enricher_processid.txt");

    EXPECT_TRUE(content.find("Process ID test") != std::string::npos);
    EXPECT_TRUE(content.find("\"processId\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"processId\":\"\"") == std::string::npos);
}

// --- machineName enricher attaches machine key (non-empty) ---

TEST_F(EnricherTest, MachineNameEnricherAttachesNonEmptyKey) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_machine.txt");
    logger.enrich(minta::Enrichers::machineName());

    logger.info("Machine name test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_machine.txt");
    std::string content = TestUtils::readLogFile("enricher_machine.txt");

    EXPECT_TRUE(content.find("Machine name test") != std::string::npos);
    EXPECT_TRUE(content.find("\"machine\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"machine\":\"\"") == std::string::npos);
}

// --- environment enricher reads $APP_ENV ---

TEST_F(EnricherTest, EnvironmentEnricherReadsAppEnv) {
#ifdef _WIN32
    _putenv_s("APP_ENV", "testing");
#else
    setenv("APP_ENV", "testing", 1);
#endif

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_env.txt");
    logger.enrich(minta::Enrichers::environment());

    logger.info("Environment test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_env.txt");
    std::string content = TestUtils::readLogFile("enricher_env.txt");

    EXPECT_TRUE(content.find("\"environment\":\"testing\"") != std::string::npos);

#ifdef _WIN32
    _putenv_s("APP_ENV", "");
#else
    unsetenv("APP_ENV");
#endif
}

// --- property enricher attaches static value ---

TEST_F(EnricherTest, PropertyEnricherAttachesStaticValue) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_property.txt");
    logger.enrich(minta::Enrichers::property("version", "2.1.0"));

    logger.info("Property test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_property.txt");
    std::string content = TestUtils::readLogFile("enricher_property.txt");

    EXPECT_TRUE(content.find("\"version\":\"2.1.0\"") != std::string::npos);
}

// --- fromEnv enricher caches env at registration ---

TEST_F(EnricherTest, FromEnvEnricherCachesAtRegistration) {
#ifdef _WIN32
    _putenv_s("MY_TEST_VAR", "original");
#else
    setenv("MY_TEST_VAR", "original", 1);
#endif

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_fromenv.txt");
    logger.enrich(minta::Enrichers::fromEnv("MY_TEST_VAR", "myVar"));

#ifdef _WIN32
    _putenv_s("MY_TEST_VAR", "changed");
#else
    setenv("MY_TEST_VAR", "changed", 1);
#endif

    logger.info("FromEnv test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_fromenv.txt");
    std::string content = TestUtils::readLogFile("enricher_fromenv.txt");

    EXPECT_TRUE(content.find("\"myVar\":\"original\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"myVar\":\"changed\"") == std::string::npos);

#ifdef _WIN32
    _putenv_s("MY_TEST_VAR", "");
#else
    unsetenv("MY_TEST_VAR");
#endif
}

// --- custom lambda enricher works ---

TEST_F(EnricherTest, CustomLambdaEnricherWorks) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_lambda.txt");
    logger.enrich([](minta::LogEntry& entry) {
        entry.customContext["custom"] = "lambda-value";
    });

    logger.info("Lambda enricher test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_lambda.txt");
    std::string content = TestUtils::readLogFile("enricher_lambda.txt");

    EXPECT_TRUE(content.find("\"custom\":\"lambda-value\"") != std::string::npos);
}

// --- multiple enrichers run in order ---

TEST_F(EnricherTest, MultipleEnrichersRunInRegistrationOrder) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_order.txt");

    logger.enrich(minta::Enrichers::property("key1", "first"));
    logger.enrich(minta::Enrichers::property("key2", "second"));
    logger.enrich([](minta::LogEntry& entry) {
        entry.customContext["order"] = "first";
    });
    logger.enrich([](minta::LogEntry& entry) {
        entry.customContext["order"] = "second";
    });

    logger.info("Order test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_order.txt");
    std::string content = TestUtils::readLogFile("enricher_order.txt");

    EXPECT_TRUE(content.find("\"key1\":\"first\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"key2\":\"second\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"order\":\"second\"") != std::string::npos);
}

// --- precedence: setContext overwrites enricher value ---

TEST_F(EnricherTest, SetContextOverwritesEnricherValue) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_prec_ctx.txt");

    logger.enrich(minta::Enrichers::property("env", "enriched"));
    logger.setContext("env", "explicit");

    logger.info("Precedence test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_prec_ctx.txt");
    std::string content = TestUtils::readLogFile("enricher_prec_ctx.txt");

    EXPECT_TRUE(content.find("\"env\":\"explicit\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"env\":\"enriched\"") == std::string::npos);

    logger.clearAllContext();
}

// --- precedence: scoped context overwrites enricher value ---

TEST_F(EnricherTest, ScopedContextOverwritesEnricherValue) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_prec_scope.txt");

    logger.enrich(minta::Enrichers::property("env", "enriched"));

    {
        auto scope = logger.scope({{"env", "scoped"}});
        logger.info("Scoped precedence test");
    }

    logger.flush();
    TestUtils::waitForFileContent("enricher_prec_scope.txt");
    std::string content = TestUtils::readLogFile("enricher_prec_scope.txt");

    EXPECT_TRUE(content.find("\"env\":\"scoped\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"env\":\"enriched\"") == std::string::npos);
}

// --- thread safety: concurrent logging with enrichers ---

TEST_F(EnricherTest, ThreadSafetyConcurrentLoggingWithEnrichers) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("enricher_threadsafe.txt");

    logger.enrich(minta::Enrichers::threadId());
    logger.enrich(minta::Enrichers::processId());
    logger.enrich(minta::Enrichers::property("app", "test"));

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    const int numThreads = 4;
    const int msgsPerThread = 20;

    auto worker = [&](int id) {
        ready.fetch_add(1);
        while (!go.load()) { std::this_thread::yield(); }
        for (int i = 0; i < msgsPerThread; ++i) {
            logger.info("Thread {tid} message {n}", std::to_string(id), std::to_string(i));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker, i);
    }

    while (ready.load() < numThreads) { std::this_thread::yield(); }
    go.store(true);

    for (auto& t : threads) {
        t.join();
    }

    logger.flush();
    TestUtils::waitForFileContent("enricher_threadsafe.txt");
    std::string content = TestUtils::readLogFile("enricher_threadsafe.txt");

    int lineCount = 0;
    for (size_t i = 0; i < content.size(); ++i) {
        if (content[i] == '\n') ++lineCount;
    }
    EXPECT_GE(lineCount, numThreads * msgsPerThread);
    EXPECT_TRUE(content.find("app=test") != std::string::npos);
    EXPECT_TRUE(content.find("threadId=") != std::string::npos);
    EXPECT_TRUE(content.find("processId=") != std::string::npos);
}

// --- enrich() throws if called after logging started ---

TEST_F(EnricherTest, EnrichThrowsAfterLoggingStarted) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("enricher_throw.txt");

    logger.info("Trigger logging started");
    logger.flush();

    EXPECT_THROW(
        logger.enrich(minta::Enrichers::threadId()),
        std::logic_error
    );
}

// --- no enrichers = no change to existing behavior ---

TEST_F(EnricherTest, NoEnrichersNoChangeToExistingBehavior) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_noop.txt");

    logger.setContext("existing", "value");
    logger.info("No enrichers");

    logger.flush();
    TestUtils::waitForFileContent("enricher_noop.txt");
    std::string content = TestUtils::readLogFile("enricher_noop.txt");

    EXPECT_TRUE(content.find("No enrichers") != std::string::npos);
    EXPECT_TRUE(content.find("\"existing\":\"value\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"threadId\"") == std::string::npos);
    EXPECT_TRUE(content.find("\"processId\"") == std::string::npos);
    EXPECT_TRUE(content.find("\"machine\"") == std::string::npos);

    logger.clearAllContext();
}

// --- caller enricher with source location ---

TEST_F(EnricherTest, CallerEnricherWithSourceLocation) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_caller.txt");
    logger.setCaptureSourceLocation(true);
    logger.enrich(minta::Enrichers::caller());

    logger.logWithSourceLocation(minta::LogLevel::INFO,
        __FILE__, __LINE__, "myTestFunction",
        "Caller test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_caller.txt");
    std::string content = TestUtils::readLogFile("enricher_caller.txt");

    EXPECT_TRUE(content.find("\"caller\":\"myTestFunction\"") != std::string::npos);
}

TEST_F(EnricherTest, CallerEnricherWithoutSourceLocationIsAbsent) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_caller_off.txt");
    logger.enrich(minta::Enrichers::caller());

    logger.info("No source location");

    logger.flush();
    TestUtils::waitForFileContent("enricher_caller_off.txt");
    std::string content = TestUtils::readLogFile("enricher_caller_off.txt");

    EXPECT_TRUE(content.find("\"caller\"") == std::string::npos);
}

// --- enrichers work with all formatter types ---

TEST_F(EnricherTest, EnrichersAppearInCompactJsonOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("enricher_cjson.txt");
    logger.enrich(minta::Enrichers::property("app", "myapp"));

    logger.info("Compact JSON enricher");

    logger.flush();
    TestUtils::waitForFileContent("enricher_cjson.txt");
    std::string content = TestUtils::readLogFile("enricher_cjson.txt");

    EXPECT_TRUE(content.find("Compact JSON enricher") != std::string::npos
             || content.find("@mt") != std::string::npos);
}

TEST_F(EnricherTest, EnrichersAppearInXmlOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("enricher_xml.txt");
    logger.enrich(minta::Enrichers::property("app", "myapp"));

    logger.info("XML enricher");

    logger.flush();
    TestUtils::waitForFileContent("enricher_xml.txt");
    std::string content = TestUtils::readLogFile("enricher_xml.txt");

    EXPECT_TRUE(content.find("XML enricher") != std::string::npos);
    EXPECT_TRUE(content.find("<app>myapp</app>") != std::string::npos);
}

TEST_F(EnricherTest, EnrichersAppearInHumanReadableOutput) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("enricher_human.txt");
    logger.enrich(minta::Enrichers::property("app", "myapp"));

    logger.info("Human readable enricher");

    logger.flush();
    TestUtils::waitForFileContent("enricher_human.txt");
    std::string content = TestUtils::readLogFile("enricher_human.txt");

    EXPECT_TRUE(content.find("Human readable enricher") != std::string::npos);
    EXPECT_TRUE(content.find("app=myapp") != std::string::npos);
}

// --- throwing enricher does not crash the pipeline ---

TEST_F(EnricherTest, ThrowingEnricherDoesNotStopSubsequentEnrichers) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_throw_safe.txt");

    logger.enrich(minta::Enrichers::property("before", "ok"));
    logger.enrich([](minta::LogEntry&) {
        throw std::runtime_error("enricher boom");
    });
    logger.enrich(minta::Enrichers::property("after", "ok"));

    logger.info("Should still appear");

    logger.flush();
    TestUtils::waitForFileContent("enricher_throw_safe.txt");
    std::string content = TestUtils::readLogFile("enricher_throw_safe.txt");

    EXPECT_TRUE(content.find("Should still appear") != std::string::npos);
    EXPECT_TRUE(content.find("\"before\":\"ok\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"after\":\"ok\"") != std::string::npos);
}

// --- environment() falls back to $ENVIRONMENT when $APP_ENV is unset ---

TEST_F(EnricherTest, EnvironmentEnricherFallsBackToEnvironmentVar) {
#ifdef _WIN32
    _putenv_s("APP_ENV", "");
    _putenv_s("ENVIRONMENT", "staging");
#else
    unsetenv("APP_ENV");
    setenv("ENVIRONMENT", "staging", 1);
#endif

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_env_fallback.txt");
    logger.enrich(minta::Enrichers::environment());

    logger.info("Fallback env test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_env_fallback.txt");
    std::string content = TestUtils::readLogFile("enricher_env_fallback.txt");

    EXPECT_TRUE(content.find("\"environment\":\"staging\"") != std::string::npos);

#ifdef _WIN32
    _putenv_s("ENVIRONMENT", "");
#else
    unsetenv("ENVIRONMENT");
#endif
}

// --- fromEnv() with non-existent env var produces empty string ---

TEST_F(EnricherTest, FromEnvMissingVarProducesEmptyString) {
#ifdef _WIN32
    _putenv_s("LUNARLOG_NONEXISTENT_VAR_XYZ", "");
#else
    unsetenv("LUNARLOG_NONEXISTENT_VAR_XYZ");
#endif

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_fromenv_missing.txt");
    logger.enrich(minta::Enrichers::fromEnv("LUNARLOG_NONEXISTENT_VAR_XYZ", "missing"));

    logger.info("Missing env var test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_fromenv_missing.txt");
    std::string content = TestUtils::readLogFile("enricher_fromenv_missing.txt");

    EXPECT_TRUE(content.find("\"missing\":\"\"") != std::string::npos);
}

// --- IEnricher interface: concrete subclass via enrich() ---

namespace {
    class AppVersionEnricher : public minta::IEnricher {
    public:
        void enrich(minta::LogEntry& entry) const override {
            entry.customContext["appVersion"] = "3.0.0";
        }
    };
}

TEST_F(EnricherTest, IEnricherConcreteSubclassWorks) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("enricher_ienricher.txt");

    auto enricher = std::make_shared<AppVersionEnricher>();
    logger.enrich([enricher](minta::LogEntry& entry) {
        enricher->enrich(entry);
    });

    logger.info("IEnricher test");

    logger.flush();
    TestUtils::waitForFileContent("enricher_ienricher.txt");
    std::string content = TestUtils::readLogFile("enricher_ienricher.txt");

    EXPECT_TRUE(content.find("\"appVersion\":\"3.0.0\"") != std::string::npos);
}
