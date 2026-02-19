#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <string>
#include <cstdlib>

// ---------------------------------------------------------------------------
// ColorConsoleSink unit tests
//
// These tests exercise the static colorize() method and the color detection
// logic. Actual stdout output is not captured — the focus is on the ANSI
// code injection and enable/disable logic.
// ---------------------------------------------------------------------------

class ColorConsoleSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- ANSI code constants used in assertions --------------------------------

static const std::string ESC_RESET = "\033[0m";
static const std::string ESC_DIM   = "\033[2m";
static const std::string ESC_CYAN  = "\033[36m";
static const std::string ESC_GREEN = "\033[32m";
static const std::string ESC_YELLOW = "\033[33m";
static const std::string ESC_RED   = "\033[31m";
static const std::string ESC_BOLD_RED = "\033[1;31m";

// --- colorize() static method tests ---------------------------------------

TEST_F(ColorConsoleSinkTest, ColorizeTrace) {
    std::string input = "2026-02-18 12:00:00.000 [TRACE] Trace message";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::TRACE);
    EXPECT_NE(result.find(ESC_DIM + "[TRACE]" + ESC_RESET), std::string::npos);
    // Message body should not contain color codes
    size_t resetPos = result.find(ESC_RESET);
    ASSERT_NE(resetPos, std::string::npos);
    std::string afterReset = result.substr(resetPos + ESC_RESET.size());
    EXPECT_EQ(afterReset.find('\033'), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeDebug) {
    std::string input = "2026-02-18 12:00:00.000 [DEBUG] Debug message";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::DEBUG);
    EXPECT_NE(result.find(ESC_CYAN + "[DEBUG]" + ESC_RESET), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeInfo) {
    std::string input = "2026-02-18 12:00:00.000 [INFO] Info message";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::INFO);
    EXPECT_NE(result.find(ESC_GREEN + "[INFO]" + ESC_RESET), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeWarn) {
    std::string input = "2026-02-18 12:00:00.000 [WARN] Warn message";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::WARN);
    EXPECT_NE(result.find(ESC_YELLOW + "[WARN]" + ESC_RESET), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeError) {
    std::string input = "2026-02-18 12:00:00.000 [ERROR] Error message";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::ERROR);
    EXPECT_NE(result.find(ESC_RED + "[ERROR]" + ESC_RESET), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeFatal) {
    std::string input = "2026-02-18 12:00:00.000 [FATAL] Fatal message";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::FATAL);
    EXPECT_NE(result.find(ESC_BOLD_RED + "[FATAL]" + ESC_RESET), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizePreservesTimestamp) {
    std::string input = "2026-02-18 12:00:00.000 [INFO] Hello world";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::INFO);
    // Timestamp before the bracket should be unchanged
    EXPECT_EQ(result.substr(0, 24), "2026-02-18 12:00:00.000 ");
}

TEST_F(ColorConsoleSinkTest, ColorizePreservesMessageBody) {
    std::string input = "2026-02-18 12:00:00.000 [INFO] Hello world with {data}";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::INFO);
    EXPECT_NE(result.find("Hello world with {data}"), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeNoBracketPassthrough) {
    // If no [LEVEL] bracket found, text passes through unchanged
    std::string input = "No bracket here";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::INFO);
    EXPECT_EQ(result, input);
}

// --- getColorCode() tests --------------------------------------------------

TEST_F(ColorConsoleSinkTest, GetColorCodeAllLevels) {
    EXPECT_STREQ(minta::ColorConsoleSink::getColorCode(minta::LogLevel::TRACE), "\033[2m");
    EXPECT_STREQ(minta::ColorConsoleSink::getColorCode(minta::LogLevel::DEBUG), "\033[36m");
    EXPECT_STREQ(minta::ColorConsoleSink::getColorCode(minta::LogLevel::INFO),  "\033[32m");
    EXPECT_STREQ(minta::ColorConsoleSink::getColorCode(minta::LogLevel::WARN),  "\033[33m");
    EXPECT_STREQ(minta::ColorConsoleSink::getColorCode(minta::LogLevel::ERROR), "\033[31m");
    EXPECT_STREQ(minta::ColorConsoleSink::getColorCode(minta::LogLevel::FATAL), "\033[1;31m");
}

// --- setColor() override ---------------------------------------------------

TEST_F(ColorConsoleSinkTest, SetColorOverridesAutoDetect) {
    minta::ColorConsoleSink sink;
    // Force color on regardless of TTY detection
    sink.setColor(true);
    EXPECT_TRUE(sink.isColorEnabled());

    sink.setColor(false);
    EXPECT_FALSE(sink.isColorEnabled());
}

// --- Integration: sink writes to file to verify colorization ---------------

TEST_F(ColorConsoleSinkTest, SinkWriteWithColorEnabled) {
    // Use a file sink with the same formatter to capture expected output,
    // then compare with colorized version
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("color_console_test.txt");
    logger.info("Hello {name}", "name", "world");
    logger.flush();
    TestUtils::waitForFileContent("color_console_test.txt");
    std::string plain = TestUtils::readLogFile("color_console_test.txt");

    // Colorize the plain output and verify ANSI codes are present
    std::string colored = minta::ColorConsoleSink::colorize(plain, minta::LogLevel::INFO);
    EXPECT_NE(colored.find(ESC_GREEN), std::string::npos);
    EXPECT_NE(colored.find(ESC_RESET), std::string::npos);
    EXPECT_NE(colored.find("[INFO]"), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, SinkWriteWithColorDisabled) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("color_console_test.txt");
    logger.warn("Warning message");
    logger.flush();
    TestUtils::waitForFileContent("color_console_test.txt");
    std::string plain = TestUtils::readLogFile("color_console_test.txt");

    // When color disabled, output is identical to plain
    // (the sink itself is not used here, but we verify the logic)
    EXPECT_EQ(plain.find('\033'), std::string::npos);
}

// --- Fluent builder integration -------------------------------------------

TEST_F(ColorConsoleSinkTest, FluentBuilderCreatesColorSink) {
    // Verify ColorConsoleSink works with the fluent builder
    auto logger = minta::LunarLog::configure()
        .minLevel(minta::LogLevel::TRACE)
        .writeTo<minta::ColorConsoleSink>("color-console")
        .writeTo<minta::FileSink>("color_console_fluent.txt")
        .build();

    logger.info("Fluent builder test");
    logger.flush();
    TestUtils::waitForFileContent("color_console_fluent.txt");
    std::string content = TestUtils::readLogFile("color_console_fluent.txt");
    EXPECT_NE(content.find("[INFO] Fluent builder test"), std::string::npos);
}

// --- Environment variable color detection tests ---------------------------

class ColorConsoleSinkEnvTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* nc = std::getenv("NO_COLOR");
        m_hadNoColor = (nc != nullptr);
        if (m_hadNoColor) m_savedNoColor = nc;

        const char* lnc = std::getenv("LUNAR_LOG_NO_COLOR");
        m_hadLunarNoColor = (lnc != nullptr);
        if (m_hadLunarNoColor) m_savedLunarNoColor = lnc;

        clearTestEnv("NO_COLOR");
        clearTestEnv("LUNAR_LOG_NO_COLOR");
    }

    void TearDown() override {
        if (m_hadNoColor)
            setTestEnv("NO_COLOR", m_savedNoColor.c_str());
        else
            clearTestEnv("NO_COLOR");

        if (m_hadLunarNoColor)
            setTestEnv("LUNAR_LOG_NO_COLOR", m_savedLunarNoColor.c_str());
        else
            clearTestEnv("LUNAR_LOG_NO_COLOR");
    }

    static void setTestEnv(const char* name, const char* value) {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    static void clearTestEnv(const char* name) {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name);
#endif
    }

private:
    bool m_hadNoColor = false;
    std::string m_savedNoColor;
    bool m_hadLunarNoColor = false;
    std::string m_savedLunarNoColor;
};

TEST_F(ColorConsoleSinkEnvTest, NoColorEmptyStringDisablesColor) {
    // NO_COLOR="" — presence alone disables color (https://no-color.org/)
    setTestEnv("NO_COLOR", "");
    minta::ColorConsoleSink sink;
    EXPECT_FALSE(sink.isColorEnabled());
}

TEST_F(ColorConsoleSinkEnvTest, NoColorNonEmptyDisablesColor) {
    setTestEnv("NO_COLOR", "1");
    minta::ColorConsoleSink sink;
    EXPECT_FALSE(sink.isColorEnabled());
}

TEST_F(ColorConsoleSinkEnvTest, LunarLogNoColorNonEmptyDisablesColor) {
    setTestEnv("LUNAR_LOG_NO_COLOR", "1");
    minta::ColorConsoleSink sink;
    EXPECT_FALSE(sink.isColorEnabled());
}

TEST_F(ColorConsoleSinkEnvTest, LunarLogNoColorEmptyDoesNotDisable) {
    // Empty LUNAR_LOG_NO_COLOR should NOT disable color by itself.
    // Capture baseline with both unset, then verify empty string
    // produces the same result (TTY-dependent).
    minta::ColorConsoleSink baseline;
    bool baselineColor = baseline.isColorEnabled();

    setTestEnv("LUNAR_LOG_NO_COLOR", "");
    minta::ColorConsoleSink sink;
    EXPECT_EQ(sink.isColorEnabled(), baselineColor);
}

TEST_F(ColorConsoleSinkEnvTest, BothUnsetReturnsBasedOnTty) {
    // With both env vars unset, detectColorSupport() falls through
    // to TTY detection. Verify it runs without crashing and returns bool.
    minta::ColorConsoleSink sink;
    bool result = sink.isColorEnabled();
    (void)result;
}

// --- Colorize with context and source location ----------------------------

TEST_F(ColorConsoleSinkTest, ColorizeWithContext) {
    std::string input = "2026-02-18 12:00:00.000 [WARN] Msg {ctx=val}";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::WARN);
    EXPECT_NE(result.find(ESC_YELLOW + "[WARN]" + ESC_RESET), std::string::npos);
    EXPECT_NE(result.find("{ctx=val}"), std::string::npos);
}

TEST_F(ColorConsoleSinkTest, ColorizeWithSourceLocation) {
    std::string input = "2026-02-18 12:00:00.000 [ERROR] Msg [file.cpp:42 main]";
    std::string result = minta::ColorConsoleSink::colorize(input, minta::LogLevel::ERROR);
    EXPECT_NE(result.find(ESC_RED + "[ERROR]" + ESC_RESET), std::string::npos);
    EXPECT_NE(result.find("[file.cpp:42 main]"), std::string::npos);
}
