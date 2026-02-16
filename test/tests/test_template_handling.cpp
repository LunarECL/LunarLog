#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <atomic>

class TemplateHandlingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- JSON messageTemplate and templateHash ---

TEST_F(TemplateHandlingTest, JsonContainsMessageTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_json_test.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    logger.flush();
    TestUtils::waitForFileContent("template_json_test.txt");
    std::string logContent = TestUtils::readLogFile("template_json_test.txt");

    EXPECT_TRUE(logContent.find("\"messageTemplate\":\"User {username} logged in from {ip}\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"User alice logged in from 192.168.1.1\"") != std::string::npos);
}

TEST_F(TemplateHandlingTest, JsonContainsTemplateHash) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_json_test.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    logger.flush();
    TestUtils::waitForFileContent("template_json_test.txt");
    std::string logContent = TestUtils::readLogFile("template_json_test.txt");

    EXPECT_TRUE(logContent.find("\"templateHash\":\"") != std::string::npos);
    // Hash is 8 hex chars
    size_t pos = logContent.find("\"templateHash\":\"");
    ASSERT_NE(pos, std::string::npos);
    pos += 16; // skip past "templateHash":"
    std::string hash = logContent.substr(pos, 8);
    EXPECT_EQ(hash.size(), 8u);
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

// --- XML MessageTemplate and hash attribute ---

TEST_F(TemplateHandlingTest, XmlContainsMessageTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("template_xml_test.txt");

    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    logger.flush();
    TestUtils::waitForFileContent("template_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("template_xml_test.txt");

    EXPECT_TRUE(logContent.find("<MessageTemplate hash=\"") != std::string::npos);
    EXPECT_TRUE(logContent.find(">User {username} logged in from {ip}</MessageTemplate>") != std::string::npos);
}

TEST_F(TemplateHandlingTest, XmlTemplateHashAttribute) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("template_xml_test.txt");

    logger.info("Test {val}", "hello");

    logger.flush();
    TestUtils::waitForFileContent("template_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("template_xml_test.txt");

    size_t pos = logContent.find("<MessageTemplate hash=\"");
    ASSERT_NE(pos, std::string::npos);
    pos += 23; // skip past <MessageTemplate hash="
    std::string hash = logContent.substr(pos, 8);
    EXPECT_EQ(hash.size(), 8u);
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

// --- Template hash consistency ---

TEST_F(TemplateHandlingTest, SameTemplateSameHash) {
    uint32_t hash1 = minta::detail::fnv1a("User {username} logged in");
    uint32_t hash2 = minta::detail::fnv1a("User {username} logged in");
    EXPECT_EQ(hash1, hash2);
}

TEST_F(TemplateHandlingTest, DifferentTemplateDifferentHash) {
    uint32_t hash1 = minta::detail::fnv1a("User {username} logged in");
    uint32_t hash2 = minta::detail::fnv1a("User {username} logged out");
    EXPECT_NE(hash1, hash2);
}

TEST_F(TemplateHandlingTest, EmptyStringHash) {
    uint32_t hash = minta::detail::fnv1a("");
    EXPECT_EQ(hash, 0x811c9dc5u);
}

TEST_F(TemplateHandlingTest, ToHexStringFormat) {
    std::string hex = minta::detail::toHexString(0xDEADBEEF);
    EXPECT_EQ(hex, "deadbeef");

    std::string hexZero = minta::detail::toHexString(0);
    EXPECT_EQ(hexZero, "00000000");
}

// --- Duplicate placeholder name warning ---

TEST_F(TemplateHandlingTest, DuplicatePlaceholderNameWarning) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("template_validation_test.txt");

    logger.info("User {name} and {name}", "alice", "bob");

    logger.flush();
    TestUtils::waitForFileContent("template_validation_test.txt");
    std::string logContent = TestUtils::readLogFile("template_validation_test.txt");

    EXPECT_TRUE(logContent.find("has duplicate placeholder name: name") != std::string::npos);
    EXPECT_TRUE(logContent.find("User {name} and {name}") != std::string::npos);
}

// --- Empty placeholder warning ---

TEST_F(TemplateHandlingTest, EmptyPlaceholderWarning) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("template_validation_test.txt");

    logger.info("Value: {}", "test");

    logger.flush();
    TestUtils::waitForFileContent("template_validation_test.txt");
    std::string logContent = TestUtils::readLogFile("template_validation_test.txt");

    EXPECT_TRUE(logContent.find("has empty placeholder") != std::string::npos);
}

// --- Whitespace-only placeholder warning ---

TEST_F(TemplateHandlingTest, WhitespaceOnlyPlaceholderWarning) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("template_validation_test.txt");

    logger.info("Value: { }", "test");

    logger.flush();
    TestUtils::waitForFileContent("template_validation_test.txt");
    std::string logContent = TestUtils::readLogFile("template_validation_test.txt");

    EXPECT_TRUE(logContent.find("has whitespace-only placeholder name") != std::string::npos);
}

// --- Validation warnings include template context ---

TEST_F(TemplateHandlingTest, ValidationWarningIncludesTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("template_validation_test.txt");

    logger.info("User {name} {name}", "a", "b");

    logger.flush();
    TestUtils::waitForFileContent("template_validation_test.txt");
    std::string logContent = TestUtils::readLogFile("template_validation_test.txt");

    EXPECT_TRUE(logContent.find("Template \"User {name} {name}\"") != std::string::npos);
}

// --- Template cache: log same template twice, verify correctness ---

TEST_F(TemplateHandlingTest, TemplateCacheHit) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_cache_test.txt");

    logger.info("Cached {val}", "first");
    logger.info("Cached {val}", "second");

    logger.flush();
    TestUtils::waitForFileContent("template_cache_test.txt");
    std::string logContent = TestUtils::readLogFile("template_cache_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"Cached first\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"Cached second\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"messageTemplate\":\"Cached {val}\"") != std::string::npos);
}

// --- Template cache disabled (size=0) ---

TEST_F(TemplateHandlingTest, TemplateCacheDisabled) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.setTemplateCacheSize(0);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_cache_test.txt");

    logger.info("NoCached {val}", "one");
    logger.info("NoCached {val}", "two");

    logger.flush();
    TestUtils::waitForFileContent("template_cache_test.txt");
    std::string logContent = TestUtils::readLogFile("template_cache_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"NoCached one\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"NoCached two\"") != std::string::npos);
}

// --- No template for plain message (no placeholders) ---

TEST_F(TemplateHandlingTest, JsonTemplateForPlainMessage) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_json_test.txt");

    logger.info("No placeholders here");

    logger.flush();
    TestUtils::waitForFileContent("template_json_test.txt");
    std::string logContent = TestUtils::readLogFile("template_json_test.txt");

    EXPECT_TRUE(logContent.find("\"messageTemplate\":\"No placeholders here\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"templateHash\":\"") != std::string::npos);
}

// --- Template with special chars is properly escaped in JSON ---

TEST_F(TemplateHandlingTest, JsonTemplateEscapesSpecialChars) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_json_test.txt");

    logger.info("Say \"hello\" to {name}", "world");

    logger.flush();
    TestUtils::waitForFileContent("template_json_test.txt");
    std::string logContent = TestUtils::readLogFile("template_json_test.txt");

    EXPECT_TRUE(logContent.find("messageTemplate") != std::string::npos);
    EXPECT_TRUE(logContent.find("\\\"hello\\\"") != std::string::npos);
}

// --- Template with special chars is properly escaped in XML ---

TEST_F(TemplateHandlingTest, XmlTemplateEscapesSpecialChars) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("template_xml_test.txt");

    logger.info("1 < 2 & {val} > 0", "x");

    logger.flush();
    TestUtils::waitForFileContent("template_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("template_xml_test.txt");

    EXPECT_TRUE(logContent.find("<MessageTemplate") != std::string::npos);
    EXPECT_TRUE(logContent.find("1 &lt; 2 &amp; {val} &gt; 0</MessageTemplate>") != std::string::npos);
}

// --- Consistent hash in JSON output across two entries ---

TEST_F(TemplateHandlingTest, JsonConsistentHashAcrossEntries) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_json_test.txt");

    logger.info("Hello {name}", "alice");
    logger.info("Hello {name}", "bob");

    logger.flush();
    TestUtils::waitForFileContent("template_json_test.txt");
    std::string logContent = TestUtils::readLogFile("template_json_test.txt");

    size_t first = logContent.find("\"templateHash\":\"");
    ASSERT_NE(first, std::string::npos);
    std::string hash1 = logContent.substr(first + 16, 8);

    size_t second = logContent.find("\"templateHash\":\"", first + 1);
    ASSERT_NE(second, std::string::npos);
    std::string hash2 = logContent.substr(second + 16, 8);

    EXPECT_EQ(hash1, hash2);
}

// --- Cache eviction: cap without eviction (L1) ---

TEST_F(TemplateHandlingTest, CacheEvictionCapPreservesHotEntries) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.setTemplateCacheSize(10);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_eviction_test.txt");

    for (int i = 0; i < 130; ++i) {
        std::ostringstream tmpl;
        tmpl << "Eviction test " << i << " {val}";
        logger.info(tmpl.str(), std::to_string(i));
    }

    logger.flush();
    TestUtils::waitForFileContent("template_eviction_test.txt");
    std::string logContent = TestUtils::readLogFile("template_eviction_test.txt");

    for (int i = 0; i < 130; ++i) {
        std::ostringstream expected;
        expected << "\"message\":\"Eviction test " << i << " " << i << "\"";
        EXPECT_TRUE(logContent.find(expected.str()) != std::string::npos)
            << "Missing message for i=" << i;
    }
}

// --- Concurrent cache access ---

TEST_F(TemplateHandlingTest, ConcurrentCacheAccess) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("template_concurrent_test.txt");

    const int numThreads = 8;
    const int logsPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<bool> go{false};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, &go, t, logsPerThread]() {
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < logsPerThread; ++i) {
                logger.info("Shared template {val}", std::to_string(t * 1000 + i));
                std::ostringstream tmpl;
                tmpl << "Thread " << t << " unique {val}";
                logger.info(tmpl.str(), std::to_string(i));
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto& th : threads) {
        th.join();
    }

    logger.flush();
    TestUtils::waitForFileContent("template_concurrent_test.txt");
    std::string logContent = TestUtils::readLogFile("template_concurrent_test.txt");

    EXPECT_FALSE(logContent.empty());
    size_t sharedCount = 0;
    size_t pos = 0;
    while ((pos = logContent.find("Shared template ", pos)) != std::string::npos) {
        ++sharedCount;
        ++pos;
    }
    EXPECT_EQ(sharedCount, static_cast<size_t>(numThreads * logsPerThread));
}

// --- setTemplateCacheSize while logging ---

TEST_F(TemplateHandlingTest, SetTemplateCacheSizeWhileLogging) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("template_resize_test.txt");

    std::atomic<bool> done{false};

    std::thread logThread([&logger, &done]() {
        for (int i = 0; !done.load(std::memory_order_acquire); ++i) {
            std::ostringstream tmpl;
            tmpl << "Resize test " << (i % 20) << " {val}";
            logger.info(tmpl.str(), std::to_string(i));
        }
    });

    for (int sz = 1; sz <= 256; sz *= 2) {
        logger.setTemplateCacheSize(static_cast<size_t>(sz));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    logger.setTemplateCacheSize(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    logger.setTemplateCacheSize(128);

    done.store(true, std::memory_order_release);
    logThread.join();

    logger.flush();
    TestUtils::waitForFileContent("template_resize_test.txt");
    std::string logContent = TestUtils::readLogFile("template_resize_test.txt");

    EXPECT_FALSE(logContent.empty());
    EXPECT_TRUE(logContent.find("Resize test") != std::string::npos);
}

// --- Operator + cache interaction ---

TEST_F(TemplateHandlingTest, OperatorCacheInteraction) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("template_opcache_test.txt");

    logger.info("{@user} logged in", "alice");
    logger.info("{@user} logged in", "bob");

    logger.info("{} is empty", "val");
    logger.info("{} is empty", "val2");

    logger.flush();
    TestUtils::waitForFileContent("template_opcache_test.txt");
    std::string logContent = TestUtils::readLogFile("template_opcache_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"alice logged in\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"bob logged in\"") != std::string::npos);

    size_t alicePropPos = logContent.find("\"user\":\"alice\"");
    EXPECT_NE(alicePropPos, std::string::npos) << "Expected destructured property for alice";
    size_t bobPropPos = logContent.find("\"user\":\"bob\"");
    EXPECT_NE(bobPropPos, std::string::npos) << "Expected destructured property for bob";

    EXPECT_TRUE(logContent.find("\"message\":\"val is empty\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"message\":\"val2 is empty\"") != std::string::npos);
}
