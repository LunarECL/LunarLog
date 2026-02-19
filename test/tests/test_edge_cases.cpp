#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "lunar_log/transport/file_transport.hpp"
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
