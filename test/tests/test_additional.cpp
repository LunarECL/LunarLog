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
        if (formatter()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_output += formatter()->format(entry) + "\n";
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

    // clampForLongLong maps NaN to 0 for hex/pad; fixed-point passes through to snprintf
    EXPECT_TRUE(logContent.find("NaN hex: 0") != std::string::npos);
    EXPECT_TRUE(logContent.find("NaN pad: 0000") != std::string::npos);
    // Fixed-point NaN: snprintf produces platform-specific nan/NaN string
    EXPECT_TRUE(logContent.find("NaN fixed: nan") != std::string::npos ||
                logContent.find("NaN fixed: -nan") != std::string::npos ||
                logContent.find("NaN fixed: NaN") != std::string::npos);
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

    EXPECT_TRUE(logContent.find("Inf fixed: inf") != std::string::npos ||
                logContent.find("Inf fixed: Inf") != std::string::npos ||
                logContent.find("Inf fixed: INF") != std::string::npos);
    EXPECT_TRUE(logContent.find("Inf sci: inf") != std::string::npos ||
                logContent.find("Inf sci: Inf") != std::string::npos ||
                logContent.find("Inf sci: INF") != std::string::npos);
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

TEST_F(AdditionalTest, UnsignedIntArgs) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    unsigned int ui = 42u;
    unsigned long ul = 123456UL;
    unsigned long long ull = 9876543210ULL;
    logger.info("ui={a} ul={b} ull={c}", ui, ul, ull);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("ui=42 ul=123456 ull=9876543210") != std::string::npos);
}

TEST_F(AdditionalTest, FlushWithNoMessages) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    auto sink = minta::detail::make_unique<TestSink>();
    TestSink* sinkPtr = sink.get();
    logger.addCustomSink(std::move(sink));

    logger.flush();

    EXPECT_TRUE(sinkPtr->getOutput().empty());
}

TEST_F(AdditionalTest, NegativePercentage) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("pct: {v:P}", -0.25);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("pct: -25.00%") != std::string::npos);
}

TEST_F(AdditionalTest, ZeroWithAllFormatSpecs) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    logger.info("f:{a:.2f} c:{b:C} x:{c:X} e:{d:e} p:{e:P} z:{f:04}", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    EXPECT_TRUE(logContent.find("f:0.00") != std::string::npos);
    EXPECT_TRUE(logContent.find("c:$0.00") != std::string::npos);
    EXPECT_TRUE(logContent.find("x:0") != std::string::npos);
    EXPECT_TRUE(logContent.find("e:0.000000e+00") != std::string::npos ||
                logContent.find("e:0.000000e+000") != std::string::npos);
    EXPECT_TRUE(logContent.find("p:0.00%") != std::string::npos);
    EXPECT_TRUE(logContent.find("z:0000") != std::string::npos);
}

TEST_F(AdditionalTest, ValuesNearLLONGMAXWithHex) {
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.addSink<minta::FileSink>("test_log.txt");

    long long nearMax = LLONG_MAX;
    logger.info("hex: {v:X}", nearMax);

    logger.flush();
    TestUtils::waitForFileContent("test_log.txt");
    std::string logContent = TestUtils::readLogFile("test_log.txt");

    // LLONG_MAX as string -> double -> clamp -> long long loses precision due to
    // double's 53-bit mantissa; the clamped value is 7FFFFFFFFFFFFC00
    EXPECT_TRUE(logContent.find("hex: 7FFFFFFFFFFFFC00") != std::string::npos);
}
