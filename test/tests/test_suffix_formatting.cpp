#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <chrono>

class SuffixFormattingTest : public ::testing::Test {};

TEST_F(SuffixFormattingTest, NoFormatSpec) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Hello {name}", "world");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Hello world") != std::string::npos);
}

TEST_F(SuffixFormattingTest, FixedPrecision) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Value: {amount:.2f}", 3.14159);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Value: 3.14") != std::string::npos);
}

TEST_F(SuffixFormattingTest, FixedPrecisionShorthand) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Pi is {pi:4f}", 3.14159);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Pi is 3.1416") != std::string::npos);
}

TEST_F(SuffixFormattingTest, CurrencyFormat) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Price: {price:C}", 42.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Price: $42.50") != std::string::npos);
}

TEST_F(SuffixFormattingTest, HexUppercase) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Hex: {val:X}", 255);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Hex: FF") != std::string::npos);
}

TEST_F(SuffixFormattingTest, HexLowercase) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Hex: {val:x}", 255);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Hex: ff") != std::string::npos);
}

TEST_F(SuffixFormattingTest, ScientificNotation) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Sci: {val:e}", 12345.6789);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("1.234568e+04") != std::string::npos ||
                output.find("1.234568e+004") != std::string::npos);
}

TEST_F(SuffixFormattingTest, Percentage) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Rate: {rate:P}", 0.856);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Rate: 85.60%") != std::string::npos);
}

TEST_F(SuffixFormattingTest, ZeroPadded) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("ID: {id:04}", 42);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("ID: 0042") != std::string::npos);
}

TEST_F(SuffixFormattingTest, NonNumericWithFormat) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Name: {name:.2f}", "alice");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Name: alice") != std::string::npos);
}

TEST_F(SuffixFormattingTest, MixedFormats) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("{user} spent {amount:C} ({pct:P})", "Bob", 99.99, 0.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Bob spent $99.99 (50.00%)") != std::string::npos);
}

TEST_F(SuffixFormattingTest, NegativeCurrency) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Loss: {val:C}", -5.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Loss: -$5.00") != std::string::npos);
}

TEST_F(SuffixFormattingTest, NegativeHex) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Neg: {val:X}", -255);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Neg: -FF") != std::string::npos);
}

TEST_F(SuffixFormattingTest, MalformedSpecFallback) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Val: {val:.f}", 3.14);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    // .f with no digits — should use default precision, not crash
    EXPECT_TRUE(output.find("Val:") != std::string::npos);
}

TEST_F(SuffixFormattingTest, EmptySpec) {
    testing::internal::CaptureStdout();
    minta::LunarLog logger(minta::LogLevel::TRACE);
    logger.info("Val: {val:}", 42);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("Val: 42") != std::string::npos);
}
