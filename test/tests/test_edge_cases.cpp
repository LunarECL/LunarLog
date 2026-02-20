#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "lunar_log/transport/file_transport.hpp"
#include "lunar_log/core/log_entry.hpp"
#include "lunar_log/log_manager.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>

class EdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(EdgeCaseTest, ToStringBoolTrue) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("Value: {val}", true);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("Value: true") != std::string::npos);
}

TEST_F(EdgeCaseTest, ToStringBoolFalse) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("Value: {val}", false);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("Value: false") != std::string::npos);
}

TEST_F(EdgeCaseTest, ClearContextSingleKey) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.setCaptureSourceLocation(true);
    logger.setContext("alpha", "aaa");
    logger.setContext("beta", "bbb");
    logger.clearContext("alpha");
    logger.info("after clearing single context");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("beta=bbb") != std::string::npos);
    EXPECT_TRUE(logContent.find("alpha") == std::string::npos);
}

TEST_F(EdgeCaseTest, EmptyMessageTemplate) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("[INFO]") != std::string::npos);
}

TEST_F(EdgeCaseTest, UnterminatedBracket) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("open {no_close", "val");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("open {no_close") != std::string::npos);
}

TEST_F(EdgeCaseTest, PrecisionCapAbove50) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("Val: {v:.999f}", 1.5);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("Val: 1.5") != std::string::npos);
    size_t valPos = logContent.find("Val: ");
    ASSERT_NE(valPos, std::string::npos);
    size_t lineEnd = logContent.find('\n', valPos);
    if (lineEnd == std::string::npos) lineEnd = logContent.size();
    std::string valStr = logContent.substr(valPos + 5, lineEnd - valPos - 5);
    size_t dotPos = valStr.find('.');
    ASSERT_NE(dotPos, std::string::npos);
    size_t decimals = valStr.size() - dotPos - 1;
    EXPECT_LE(decimals, 50u);
}

TEST_F(EdgeCaseTest, ZeroPadNegativeNumber) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("Neg: {v:04}", -5);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("Neg: -0005") != std::string::npos);
}

TEST_F(EdgeCaseTest, ConstructorWithoutDefaultConsoleSink) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("no console sink");

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(logContent.find("no console sink") != std::string::npos);
}

// ---------------------------------------------------------------------------
// FileTransport direct construction tests (coverage for file_transport.hpp)
// ---------------------------------------------------------------------------

TEST_F(EdgeCaseTest, FileTransportInvalidPathThrows) {
    EXPECT_THROW(
        minta::FileTransport("/nonexistent/path/file.log"),
        std::runtime_error
    );
}

TEST_F(EdgeCaseTest, FileTransportValidPathWriteSucceeds) {
    {
        minta::FileTransport transport("test_log.txt");
        transport.write("hello from FileTransport");
    }
    std::string content = TestUtils::readLogFile("test_log.txt");
    EXPECT_TRUE(content.find("hello from FileTransport") != std::string::npos);
}

TEST_F(EdgeCaseTest, CloneEntryWithException) {
    minta::LogEntry src;
    src.level = minta::LogLevel::ERROR;
    src.message = "exception test";
    src.templateStr = "exception test";
    src.timestamp = std::chrono::system_clock::now();
    src.file = "test.cpp";
    src.line = 42;
    src.function = "testFunc";
    src.exception.reset(new minta::detail::ExceptionInfo());
    src.exception->type = "std::runtime_error";
    src.exception->message = "something went wrong";
    src.exception->chain = "std::logic_error: inner cause";

    minta::LogEntry dst = minta::detail::cloneEntry(src);

    EXPECT_EQ(dst.level, minta::LogLevel::ERROR);
    EXPECT_EQ(dst.message, "exception test");
    EXPECT_EQ(dst.file, "test.cpp");
    EXPECT_EQ(dst.line, 42);
    EXPECT_EQ(dst.function, "testFunc");
    ASSERT_TRUE(dst.hasException());
    EXPECT_EQ(dst.exception->type, "std::runtime_error");
    EXPECT_EQ(dst.exception->message, "something went wrong");
    EXPECT_EQ(dst.exception->chain, "std::logic_error: inner cause");
    EXPECT_NE(&src.exception, &dst.exception);
}

TEST_F(EdgeCaseTest, CloneEntryWithoutException) {
    minta::LogEntry src;
    src.level = minta::LogLevel::INFO;
    src.message = "no exception";
    src.templateStr = "no exception";
    src.timestamp = std::chrono::system_clock::now();
    src.tags.push_back("tag1");
    src.tags.push_back("tag2");
    src.customContext["key1"] = "val1";
    src.locale = "en_US";

    minta::LogEntry dst = minta::detail::cloneEntry(src);

    EXPECT_EQ(dst.message, "no exception");
    EXPECT_FALSE(dst.hasException());
    ASSERT_EQ(dst.tags.size(), 2u);
    EXPECT_EQ(dst.tags[0], "tag1");
    EXPECT_EQ(dst.tags[1], "tag2");
    EXPECT_EQ(dst.customContext.at("key1"), "val1");
    EXPECT_EQ(dst.locale, "en_US");
}

TEST_F(EdgeCaseTest, LogEntryHasExceptionBothPaths) {
    minta::LogEntry e1;
    EXPECT_FALSE(e1.hasException());

    minta::LogEntry e2;
    e2.exception.reset(new minta::detail::ExceptionInfo());
    e2.exception->type = "Error";
    EXPECT_TRUE(e2.hasException());
}

TEST_F(EdgeCaseTest, LogEntryMoveConstructor) {
    minta::LogEntry src;
    src.level = minta::LogLevel::WARN;
    src.message = "move me";
    src.templateStr = "move me";
    src.timestamp = std::chrono::system_clock::now();
    src.exception.reset(new minta::detail::ExceptionInfo());
    src.exception->type = "MoveTest";
    src.exception->message = "moved";

    minta::LogEntry dst(std::move(src));

    EXPECT_EQ(dst.level, minta::LogLevel::WARN);
    EXPECT_EQ(dst.message, "move me");
    ASSERT_TRUE(dst.hasException());
    EXPECT_EQ(dst.exception->type, "MoveTest");
    EXPECT_FALSE(src.hasException());
}

TEST_F(EdgeCaseTest, LogEntryMoveAssignment) {
    minta::LogEntry src;
    src.level = minta::LogLevel::FATAL;
    src.message = "assign me";
    src.exception.reset(new minta::detail::ExceptionInfo());
    src.exception->message = "assigned";

    minta::LogEntry dst;
    dst = std::move(src);

    EXPECT_EQ(dst.level, minta::LogLevel::FATAL);
    EXPECT_EQ(dst.message, "assign me");
    ASSERT_TRUE(dst.hasException());
    EXPECT_EQ(dst.exception->message, "assigned");
}

TEST_F(EdgeCaseTest, LogEntryPositionalConstructor) {
    auto now = std::chrono::system_clock::now();
    std::vector<std::pair<std::string, std::string>> args = {{"name", "alice"}};
    std::map<std::string, std::string> ctx = {{"req", "123"}};
    std::vector<minta::PlaceholderProperty> props;
    minta::PlaceholderProperty pp;
    pp.name = "name";
    pp.value = "alice";
    pp.op = '@';
    props.push_back(pp);
    std::vector<std::string> tags = {"audit"};

    minta::LogEntry entry(
        minta::LogLevel::DEBUG,
        "Hello alice",
        now,
        "Hello {name}",
        12345,
        args,
        "main.cpp",
        10,
        "main",
        ctx,
        props,
        tags,
        "de_DE",
        std::this_thread::get_id()
    );

    EXPECT_EQ(entry.level, minta::LogLevel::DEBUG);
    EXPECT_EQ(entry.message, "Hello alice");
    EXPECT_EQ(entry.templateStr, "Hello {name}");
    EXPECT_EQ(entry.templateHash, 12345u);
    EXPECT_EQ(entry.file, "main.cpp");
    EXPECT_EQ(entry.line, 10);
    EXPECT_EQ(entry.function, "main");
    EXPECT_EQ(entry.locale, "de_DE");
    ASSERT_EQ(entry.tags.size(), 1u);
    EXPECT_EQ(entry.tags[0], "audit");
    EXPECT_EQ(entry.customContext.at("req"), "123");
    ASSERT_EQ(entry.properties.size(), 1u);
    EXPECT_EQ(entry.properties[0].op, '@');
}

TEST_F(EdgeCaseTest, LogManagerMoveConstructor) {
    minta::LogManager mgr;
    mgr.addSink(minta::detail::make_unique<minta::FileSink>("test_log.txt"));
    EXPECT_FALSE(mgr.isLoggingStarted());

    minta::LogManager moved(std::move(mgr));
    EXPECT_FALSE(moved.isLoggingStarted());
    EXPECT_NO_THROW(moved.getSink(0));
}
