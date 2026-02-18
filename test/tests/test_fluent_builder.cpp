#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>
#include <thread>

namespace {

// Sink that is both default-constructible and string-constructible.
// Used to verify SFINAE correctly disambiguates named vs unnamed overloads.
class FlexiSink : public minta::BaseSink {
public:
    FlexiSink() {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
        setTransport(minta::detail::make_unique<minta::StdoutTransport>());
    }
    explicit FlexiSink(const std::string& filename) {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
        setTransport(minta::detail::make_unique<minta::FileTransport>(filename));
    }
};

} // anonymous namespace

class FluentBuilderTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(FluentBuilderTest, MinimalBuilder) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("fb_minimal.txt")
        .build();

    log.info("Hello from builder");
    log.flush();
    TestUtils::waitForFileContent("fb_minimal.txt");
    std::string content = TestUtils::readLogFile("fb_minimal.txt");
    EXPECT_TRUE(content.find("Hello from builder") != std::string::npos);
}

TEST_F(FluentBuilderTest, MinLevelViaBuilder) {
    auto log = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::WARN)
        .writeTo<minta::FileSink>("fb_minlevel.txt")
        .build();

    log.info("Should not appear");
    log.warn("Should appear");
    log.flush();
    TestUtils::waitForFileContent("fb_minlevel.txt");
    std::string content = TestUtils::readLogFile("fb_minlevel.txt");
    EXPECT_TRUE(content.find("Should not appear") == std::string::npos);
    EXPECT_TRUE(content.find("Should appear") != std::string::npos);
}

TEST_F(FluentBuilderTest, CaptureSourceLocationViaBuilder) {
    auto log = minta::LunarLog::configure()
        .captureSourceLocation(true)
        .writeTo<minta::FileSink, minta::JsonFormatter>("fb_srcloc.txt")
        .build();

    log.logWithSourceLocation(minta::LogLevel::INFO, "test.cpp", 42, "testFunc", "Source loc test");
    log.flush();
    TestUtils::waitForFileContent("fb_srcloc.txt");
    std::string content = TestUtils::readLogFile("fb_srcloc.txt");
    EXPECT_TRUE(content.find("test.cpp") != std::string::npos);
    EXPECT_TRUE(content.find("testFunc") != std::string::npos);
}

TEST_F(FluentBuilderTest, RateLimitViaBuilder) {
    auto log = minta::LunarLog::configure()
        .rateLimit(5, std::chrono::seconds(1))
        .writeTo<minta::FileSink>("fb_ratelimit.txt")
        .build();

    for (int i = 0; i < 20; ++i) {
        log.info("Message {i}", i);
    }
    log.flush();
    TestUtils::waitForFileContent("fb_ratelimit.txt");
    std::string content = TestUtils::readLogFile("fb_ratelimit.txt");

    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("Message ", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    EXPECT_LE(count, 6u);
}

TEST_F(FluentBuilderTest, TemplateCacheSizeViaBuilder) {
    auto log = minta::LunarLog::configure()
        .templateCacheSize(16)
        .writeTo<minta::FileSink>("fb_cache.txt")
        .build();

    log.info("Cache test {val}", 42);
    log.flush();
    TestUtils::waitForFileContent("fb_cache.txt");
    std::string content = TestUtils::readLogFile("fb_cache.txt");
    EXPECT_TRUE(content.find("Cache test 42") != std::string::npos);
}

TEST_F(FluentBuilderTest, EnrichViaBuilder) {
    auto log = minta::LunarLog::configure()
        .enrich(minta::Enrichers::property("service", "test-svc"))
        .enrich(minta::Enrichers::threadId())
        .writeTo<minta::FileSink, minta::JsonFormatter>("fb_enrich.txt")
        .build();

    log.info("Enriched entry");
    log.flush();
    TestUtils::waitForFileContent("fb_enrich.txt");
    std::string content = TestUtils::readLogFile("fb_enrich.txt");
    EXPECT_TRUE(content.find("test-svc") != std::string::npos);
    EXPECT_TRUE(content.find("threadId") != std::string::npos);
}

TEST_F(FluentBuilderTest, FilterViaBuilder) {
    auto log = minta::LunarLog::configure()
        .filter("WARN+")
        .writeTo<minta::FileSink>("fb_filter.txt")
        .build();

    log.info("Should be filtered");
    log.warn("Should pass");
    log.flush();
    TestUtils::waitForFileContent("fb_filter.txt");
    std::string content = TestUtils::readLogFile("fb_filter.txt");
    EXPECT_TRUE(content.find("Should be filtered") == std::string::npos);
    EXPECT_TRUE(content.find("Should pass") != std::string::npos);
}

TEST_F(FluentBuilderTest, FilterRuleViaBuilder) {
    auto log = minta::LunarLog::configure()
        .filterRule("level >= ERROR")
        .writeTo<minta::FileSink>("fb_filterrule.txt")
        .build();

    log.warn("Warn should be filtered");
    log.error("Error should pass");
    log.flush();
    TestUtils::waitForFileContent("fb_filterrule.txt");
    std::string content = TestUtils::readLogFile("fb_filterrule.txt");
    EXPECT_TRUE(content.find("Warn should be filtered") == std::string::npos);
    EXPECT_TRUE(content.find("Error should pass") != std::string::npos);
}

TEST_F(FluentBuilderTest, NamedSinkWriteTo) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("app", "fb_named_app.txt")
        .writeTo<minta::FileSink>("errors", "fb_named_errors.txt")
        .build();

    log.sink("errors").level(minta::LogLevel::ERROR);

    log.info("Info message");
    log.error("Error message");
    log.flush();

    TestUtils::waitForFileContent("fb_named_app.txt");
    TestUtils::waitForFileContent("fb_named_errors.txt");

    std::string appContent = TestUtils::readLogFile("fb_named_app.txt");
    std::string errContent = TestUtils::readLogFile("fb_named_errors.txt");

    EXPECT_TRUE(appContent.find("Info message") != std::string::npos);
    EXPECT_TRUE(appContent.find("Error message") != std::string::npos);
    EXPECT_TRUE(errContent.find("Info message") == std::string::npos);
    EXPECT_TRUE(errContent.find("Error message") != std::string::npos);
}

TEST_F(FluentBuilderTest, WriteToWithLambda) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("configured", [](minta::SinkProxy& s) {
            s.level(minta::LogLevel::ERROR);
            s.formatter(minta::detail::make_unique<minta::JsonFormatter>());
        }, "fb_lambda.txt")
        .build();

    log.info("Should be filtered by sink level");
    log.error("Should pass as JSON");
    log.flush();

    TestUtils::waitForFileContent("fb_lambda.txt");
    std::string content = TestUtils::readLogFile("fb_lambda.txt");
    EXPECT_TRUE(content.find("Should be filtered by sink level") == std::string::npos);
    EXPECT_TRUE(content.find("\"level\":\"ERROR\"") != std::string::npos);
    EXPECT_TRUE(content.find("Should pass as JSON") != std::string::npos);
}

TEST_F(FluentBuilderTest, UnnamedWriteTo) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("fb_unnamed.txt")
        .build();

    log.info("Unnamed sink test");
    log.flush();
    TestUtils::waitForFileContent("fb_unnamed.txt");
    std::string content = TestUtils::readLogFile("fb_unnamed.txt");
    EXPECT_TRUE(content.find("Unnamed sink test") != std::string::npos);

    // Auto-named sinks should be accessible via sink_0
    log.sink("sink_0").level(minta::LogLevel::ERROR);
    log.info("Should be filtered now");
    log.error("Error after level change");
    log.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    content = TestUtils::readLogFile("fb_unnamed.txt");
    EXPECT_TRUE(content.find("Should be filtered now") == std::string::npos);
    EXPECT_TRUE(content.find("Error after level change") != std::string::npos);
}

TEST_F(FluentBuilderTest, BuildTwiceThrows) {
    auto config = minta::LunarLog::configure();
    config.writeTo<minta::FileSink>("fb_buildtwice.txt");

    auto log = config.build();
    EXPECT_THROW(config.build(), std::logic_error);
}

TEST_F(FluentBuilderTest, FullBuilderChain) {
    auto log = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .captureSourceLocation(true)
        .rateLimit(1000, std::chrono::seconds(1))
        .templateCacheSize(256)
        .enrich(minta::Enrichers::property("service", "payment-api"))
        .filter("WARN+")
        .writeTo<minta::ConsoleSink>("console")
        .writeTo<minta::FileSink>("app", "fb_full_app.txt")
        .writeTo<minta::FileSink>("errors", [](minta::SinkProxy& s) {
            s.level(minta::LogLevel::ERROR);
            s.formatter(minta::detail::make_unique<minta::JsonFormatter>());
        }, "fb_full_errors.txt")
        .build();

    log.warn("Warning message {code}", 503);
    log.error("Error message {detail}", "timeout");
    log.flush();

    TestUtils::waitForFileContent("fb_full_app.txt");
    TestUtils::waitForFileContent("fb_full_errors.txt");

    std::string appContent = TestUtils::readLogFile("fb_full_app.txt");
    std::string errContent = TestUtils::readLogFile("fb_full_errors.txt");

    EXPECT_TRUE(appContent.find("Warning message 503") != std::string::npos);
    EXPECT_TRUE(appContent.find("Error message timeout") != std::string::npos);
    EXPECT_TRUE(errContent.find("Warning message") == std::string::npos);
    EXPECT_TRUE(errContent.find("\"level\":\"ERROR\"") != std::string::npos);
    EXPECT_TRUE(errContent.find("payment-api") != std::string::npos);
}

TEST_F(FluentBuilderTest, ExistingImperativeApiStillWorks) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("fb_imperative.txt");
    logger.setMinLevel(minta::LogLevel::DEBUG);
    logger.info("Imperative API works");
    logger.flush();

    TestUtils::waitForFileContent("fb_imperative.txt");
    std::string content = TestUtils::readLogFile("fb_imperative.txt");
    EXPECT_TRUE(content.find("Imperative API works") != std::string::npos);
}

TEST_F(FluentBuilderTest, BuilderAndImperativeCoexist) {
    auto builderLog = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("fb_coexist_builder.txt")
        .build();

    minta::LunarLog imperativeLog(minta::LogLevel::INFO, false);
    imperativeLog.addSink<minta::FileSink>("fb_coexist_imperative.txt");

    builderLog.info("From builder");
    imperativeLog.info("From imperative");
    builderLog.flush();
    imperativeLog.flush();

    TestUtils::waitForFileContent("fb_coexist_builder.txt");
    TestUtils::waitForFileContent("fb_coexist_imperative.txt");

    std::string builderContent = TestUtils::readLogFile("fb_coexist_builder.txt");
    std::string imperativeContent = TestUtils::readLogFile("fb_coexist_imperative.txt");

    EXPECT_TRUE(builderContent.find("From builder") != std::string::npos);
    EXPECT_TRUE(imperativeContent.find("From imperative") != std::string::npos);
    EXPECT_TRUE(builderContent.find("From imperative") == std::string::npos);
    EXPECT_TRUE(imperativeContent.find("From builder") == std::string::npos);
}

TEST_F(FluentBuilderTest, LocaleViaBuilder) {
    auto log = minta::LunarLog::configure()
        .locale("C")
        .writeTo<minta::FileSink>("fb_locale.txt")
        .build();

    log.info("Locale test");
    log.flush();
    TestUtils::waitForFileContent("fb_locale.txt");
    std::string content = TestUtils::readLogFile("fb_locale.txt");
    EXPECT_TRUE(content.find("Locale test") != std::string::npos);
}

TEST_F(FluentBuilderTest, MultipleUnnamedSinks) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("fb_multi_unnamed_a.txt")
        .writeTo<minta::FileSink>("fb_multi_unnamed_b.txt")
        .build();

    log.info("Multi unnamed");
    log.flush();

    TestUtils::waitForFileContent("fb_multi_unnamed_a.txt");
    TestUtils::waitForFileContent("fb_multi_unnamed_b.txt");

    std::string a = TestUtils::readLogFile("fb_multi_unnamed_a.txt");
    std::string b = TestUtils::readLogFile("fb_multi_unnamed_b.txt");

    EXPECT_TRUE(a.find("Multi unnamed") != std::string::npos);
    EXPECT_TRUE(b.find("Multi unnamed") != std::string::npos);
}

TEST_F(FluentBuilderTest, WriteToWithJsonFormatter) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("json", [](minta::SinkProxy& s) {
            s.formatter(minta::detail::make_unique<minta::JsonFormatter>());
        }, "fb_json_fmt.txt")
        .build();

    log.info("JSON via builder {key}", "val");
    log.flush();
    TestUtils::waitForFileContent("fb_json_fmt.txt");
    std::string content = TestUtils::readLogFile("fb_json_fmt.txt");
    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos);
    EXPECT_TRUE(content.find("JSON via builder val") != std::string::npos);
}

TEST_F(FluentBuilderTest, NamedWriteToWithFormatterType) {
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink, minta::JsonFormatter>(
            "json-sink", "fb_named_json_fmt.txt")
        .build();

    log.info("Named formatter test {key}", "value");
    log.flush();
    TestUtils::waitForFileContent("fb_named_json_fmt.txt");
    std::string content = TestUtils::readLogFile("fb_named_json_fmt.txt");
    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos);
    EXPECT_TRUE(content.find("Named formatter test value") != std::string::npos);

    log.sink("json-sink").level(minta::LogLevel::ERROR);
    log.info("Should be filtered after level change");
    log.error("Error after level change");
    log.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    content = TestUtils::readLogFile("fb_named_json_fmt.txt");
    EXPECT_TRUE(content.find("Should be filtered after level change") == std::string::npos);
    EXPECT_TRUE(content.find("Error after level change") != std::string::npos);
}

TEST_F(FluentBuilderTest, SfinaeDisambiguatesStringCtorSink) {
    // FlexiSink is constructible from both () and (const string&).
    // Without SFINAE, writeTo<FlexiSink>("fb_sfinae.txt") would be ambiguous:
    // named overload sees name="fb_sfinae.txt" + default ctor,
    // unnamed overload sees ctor arg="fb_sfinae.txt".
    // The SFINAE guard ensures the unnamed overload wins.
    auto log = minta::LunarLog::configure()
        .writeTo<FlexiSink>("fb_sfinae.txt")
        .build();

    log.info("SFINAE disambiguation test");
    log.flush();
    TestUtils::waitForFileContent("fb_sfinae.txt");
    std::string content = TestUtils::readLogFile("fb_sfinae.txt");
    EXPECT_TRUE(content.find("SFINAE disambiguation test") != std::string::npos);
}

TEST_F(FluentBuilderTest, SfinaeNamedStringCtorSink) {
    // When a second argument is present that isn't a valid ctor arg alongside
    // the name, the named overload correctly matches.
    auto log = minta::LunarLog::configure()
        .writeTo<minta::FileSink>("my-file-sink", "fb_sfinae_named.txt")
        .build();

    log.info("Named SFINAE test");
    log.flush();
    TestUtils::waitForFileContent("fb_sfinae_named.txt");
    std::string content = TestUtils::readLogFile("fb_sfinae_named.txt");
    EXPECT_TRUE(content.find("Named SFINAE test") != std::string::npos);

    // Verify the sink is accessible by name
    log.sink("my-file-sink").level(minta::LogLevel::ERROR);
    log.info("Should be filtered");
    log.error("Error passes");
    log.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    content = TestUtils::readLogFile("fb_sfinae_named.txt");
    EXPECT_TRUE(content.find("Should be filtered") == std::string::npos);
    EXPECT_TRUE(content.find("Error passes") != std::string::npos);
}

TEST_F(FluentBuilderTest, BuildWithNoSinksProducesSilentLogger) {
    auto log = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::DEBUG)
        .build();

    // Should not crash — just silently discards all messages
    log.info("This goes nowhere");
    log.flush();
}

TEST_F(FluentBuilderTest, SetRateLimitImperativeApi) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("fb_ratelimit_imp.txt");
    logger.setRateLimit(5, std::chrono::seconds(60));

    for (int i = 0; i < 20; ++i) {
        logger.info("RateMsg {i}", i);
    }
    logger.flush();
    TestUtils::waitForFileContent("fb_ratelimit_imp.txt");
    std::string content = TestUtils::readLogFile("fb_ratelimit_imp.txt");

    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find("RateMsg ", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    EXPECT_LE(count, 6u);
}
