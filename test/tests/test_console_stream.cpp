#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <string>

// ConsoleStream tests verify that:
// - The ConsoleStream enum compiles and has StdOut/StdErr values
// - ConsoleSink and ColorConsoleSink accept the enum without crashing
// - Default behavior (no arg) remains unchanged
// Note: Actual stream routing (stdout vs stderr) is not capture-tested here
// because StdoutTransport uses std::cout which is independent of C's stdout
// FILE*, making freopen-based capture unreliable cross-platform.

class ConsoleStreamTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(ConsoleStreamTest, ConsoleStreamEnumExists) {
    minta::ConsoleStream out = minta::ConsoleStream::StdOut;
    minta::ConsoleStream err = minta::ConsoleStream::StdErr;
    EXPECT_NE(static_cast<int>(out), static_cast<int>(err));
}

TEST_F(ConsoleStreamTest, ConsoleSinkStdOutConstructsAndLogs) {
    EXPECT_NO_THROW({
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::INFO)
            .writeTo<minta::ConsoleSink>(minta::ConsoleStream::StdOut)
            .build();
        logger.info("StdOut sink test");
        logger.flush();
    });
}

TEST_F(ConsoleStreamTest, ConsoleSinkStdErrConstructsAndLogs) {
    EXPECT_NO_THROW({
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::INFO)
            .writeTo<minta::ConsoleSink>(minta::ConsoleStream::StdErr)
            .build();
        logger.info("StdErr sink test");
        logger.flush();
    });
}

TEST_F(ConsoleStreamTest, ConsoleSinkDefaultConstructsAndLogs) {
    EXPECT_NO_THROW({
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::INFO)
            .writeTo<minta::ConsoleSink>()
            .build();
        logger.info("Default ConsoleSink test");
        logger.flush();
    });
}

TEST_F(ConsoleStreamTest, ColorConsoleSinkStdOutConstructsAndLogs) {
    EXPECT_NO_THROW({
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::INFO)
            .writeTo<minta::ColorConsoleSink>(minta::ConsoleStream::StdOut)
            .build();
        logger.info("Color StdOut test");
        logger.flush();
    });
}

TEST_F(ConsoleStreamTest, ColorConsoleSinkStdErrConstructsAndLogs) {
    EXPECT_NO_THROW({
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::INFO)
            .writeTo<minta::ColorConsoleSink>(minta::ConsoleStream::StdErr)
            .build();
        logger.info("Color StdErr test");
        logger.flush();
    });
}

TEST_F(ConsoleStreamTest, ColorConsoleSinkDefaultConstructsAndLogs) {
    EXPECT_NO_THROW({
        auto logger = minta::LunarLog::configure()
            .minLevel(minta::LogLevel::INFO)
            .writeTo<minta::ColorConsoleSink>()
            .build();
        logger.info("Color default test");
        logger.flush();
    });
}
