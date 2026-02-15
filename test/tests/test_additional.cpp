#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <cmath>
#include <limits>
#include <sstream>

class AdditionalTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

class TestSink : public minta::ISink {
public:
    TestSink() {
        setFormatter(minta::detail::make_unique<minta::HumanReadableFormatter>());
    }

    void write(const minta::LogEntry &entry) override {
        if (m_formatter) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_output += m_formatter->format(entry) + "\n";
        }
    }

    std::string getOutput() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_output;
    }

private:
    std::string m_output;
    mutable std::mutex m_mutex;
};

TEST_F(AdditionalTest, AddCustomSink) {
    auto sink = minta::detail::make_unique<TestSink>();
    TestSink* sinkPtr = sink.get();

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addCustomSink(std::move(sink));

    logger.info("custom sink message");
    logger.flush();

    std::string output = sinkPtr->getOutput();
    EXPECT_TRUE(output.find("custom sink message") != std::string::npos);
}

TEST_F(AdditionalTest, LogWithSourceLocationMacro) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("source_loc_test.txt");
    logger.setCaptureSourceLocation(true);

    logger.logWithSourceLocation(minta::LogLevel::INFO, LUNAR_LOG_CONTEXT, "source loc test {v}", 42);

    logger.flush();
    TestUtils::waitForFileContent("source_loc_test.txt");
    std::string logContent = TestUtils::readLogFile("source_loc_test.txt");

    EXPECT_TRUE(logContent.find("source loc test 42") != std::string::npos);
    EXPECT_TRUE(logContent.find("test_additional.cpp") != std::string::npos);
}

TEST_F(AdditionalTest, FileTransportOpenFailure) {
    EXPECT_THROW(
        minta::FileTransport("/nonexistent/path/to/file.log"),
        std::runtime_error
    );
}

struct CustomPoint {
    int x, y;
};

inline std::ostream& operator<<(std::ostream& os, const CustomPoint& p) {
    os << "(" << p.x << "," << p.y << ")";
    return os;
}

TEST_F(AdditionalTest, CustomTypeWithOperatorStream) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    CustomPoint pt{3, 7};
    logger.info("Point: {p}", pt);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("Point: (3,7)") != std::string::npos);
}

TEST_F(AdditionalTest, NaNWithFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    double nanVal = std::numeric_limits<double>::quiet_NaN();
    logger.info("NaN fixed: {v:.2f}", nanVal);
    logger.info("NaN hex: {v:X}", nanVal);
    logger.info("NaN pad: {v:04}", nanVal);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("NaN fixed:") != std::string::npos);
    EXPECT_TRUE(logContent.find("NaN hex:") != std::string::npos);
    EXPECT_TRUE(logContent.find("NaN pad:") != std::string::npos);
}

TEST_F(AdditionalTest, InfWithFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    double infVal = std::numeric_limits<double>::infinity();
    logger.info("Inf fixed: {v:.2f}", infVal);
    logger.info("Inf sci: {v:e}", infVal);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("Inf fixed:") != std::string::npos);
    EXPECT_TRUE(logContent.find("Inf sci:") != std::string::npos);
}

TEST_F(AdditionalTest, MultipleColonsInPlaceholderName) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("Value: {ns:sub:val:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("Value: ") != std::string::npos);
}

TEST_F(AdditionalTest, AddSinkAfterLoggingThrows) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("first message");
    logger.flush();

    EXPECT_THROW(
        logger.addSink<minta::FileSink>("test_log.txt"),
        std::logic_error
    );
}

TEST_F(AdditionalTest, DeprecatedLogWithContextStillWorks) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("source_loc_test.txt");
    logger.setCaptureSourceLocation(true);

    logger.logWithContext(minta::LogLevel::INFO, LUNAR_LOG_CONTEXT, "deprecated call {v}", 99);

    logger.flush();
    TestUtils::waitForFileContent("source_loc_test.txt");
    std::string logContent = TestUtils::readLogFile("source_loc_test.txt");

    EXPECT_TRUE(logContent.find("deprecated call 99") != std::string::npos);
}
