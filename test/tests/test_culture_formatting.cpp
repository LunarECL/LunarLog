#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <ctime>
#include <locale>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Check whether a locale name can be created via the same logic the library
/// uses (name as-is, then name + ".UTF-8", then fallback to classic).
static bool isLocaleAvailable(const std::string& name) {
    try {
        std::locale loc(name);
        return true;
    } catch (...) {}
    if (name.find('.') == std::string::npos) {
        try {
            std::locale loc(name + ".UTF-8");
            return true;
        } catch (...) {}
    }
    return false;
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CultureFormattingTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// ===========================================================================
// 1. Locale number formatting (:n, :N) with different locales
// ===========================================================================

TEST_F(CultureFormattingTest, NumberFormatWithCLocale) {
    // With "C" locale (default), :n should produce the value unchanged
    // (no thousand separators, '.' as decimal)
    std::string result = minta::detail::applyFormat("1234567.89", "n", "C");
    EXPECT_EQ(result, "1234567.89");
}

TEST_F(CultureFormattingTest, NumberFormatUpperN_CLocale) {
    std::string result = minta::detail::applyFormat("1234567.89", "N", "C");
    EXPECT_EQ(result, "1234567.89");
}

TEST_F(CultureFormattingTest, NumberFormatNonNumericPassthrough) {
    // Non-numeric value should be returned as-is
    std::string result = minta::detail::applyFormat("hello", "n", "de_DE");
    EXPECT_EQ(result, "hello");
}

TEST_F(CultureFormattingTest, NumberFormatIntegerNoDecimals) {
    // Integer values should not get spurious decimals
    std::string result = minta::detail::applyFormat("42", "n", "C");
    EXPECT_EQ(result, "42");
}

TEST_F(CultureFormattingTest, NumberFormatDE_DE) {
    if (!isLocaleAvailable("de_DE")) {
        GTEST_SKIP() << "de_DE locale not available on this system";
    }
    // German locale: decimal separator is ',' and thousand separator is '.'
    std::string result = minta::detail::applyFormat("1234567.89", "n", "de_DE");
    // Must contain the German decimal comma
    EXPECT_NE(result.find(','), std::string::npos)
        << "Expected German decimal comma in: " << result;
    // Must contain the German thousand separator (period)
    EXPECT_NE(result.find('.'), std::string::npos)
        << "Expected German thousand separator in: " << result;
}

TEST_F(CultureFormattingTest, NumberFormatFR_FR) {
    if (!isLocaleAvailable("fr_FR")) {
        GTEST_SKIP() << "fr_FR locale not available on this system";
    }
    std::string result = minta::detail::applyFormat("1234567.89", "n", "fr_FR");
    // French locale uses comma as decimal separator
    EXPECT_NE(result.find(','), std::string::npos)
        << "Expected French decimal comma in: " << result;
}

TEST_F(CultureFormattingTest, NumberFormatEN_US) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }
    std::string result = minta::detail::applyFormat("1234567.89", "n", "en_US");
    // US locale uses '.' for decimal
    EXPECT_NE(result.find('.'), std::string::npos)
        << "Expected US decimal point in: " << result;
    // US locale uses ',' for thousands
    EXPECT_NE(result.find(','), std::string::npos)
        << "Expected US thousand comma in: " << result;
}

TEST_F(CultureFormattingTest, NumberFormatThroughLogger) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_number_test.txt");

    // Default (no locale set) -> "C" behavior
    logger.info("Value: {val:n}", 1234567.89);

    logger.flush();
    TestUtils::waitForFileContent("culture_number_test.txt");
    std::string content = TestUtils::readLogFile("culture_number_test.txt");
    EXPECT_NE(content.find("Value: 1234567.89"), std::string::npos)
        << "Default locale should produce C-style number. Got: " << content;
}

TEST_F(CultureFormattingTest, NumberFormatWithLoggerLocale) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_number_locale_test.txt");
    logger.setLocale("en_US");

    logger.info("Value: {val:n}", 1234567.89);

    logger.flush();
    TestUtils::waitForFileContent("culture_number_locale_test.txt");
    std::string content = TestUtils::readLogFile("culture_number_locale_test.txt");
    // en_US should produce thousand-separated number with commas
    EXPECT_NE(content.find(','), std::string::npos)
        << "en_US locale should add thousand separators. Got: " << content;
}

// ===========================================================================
// 2. Date/time formatting (:d, :D, :t, :T, :f, :F) from unix timestamps
// ===========================================================================

// Use a known timestamp: 2024-01-15 10:30:45 UTC = 1705312245
static const std::string kTestTimestamp = "1705312245";

TEST_F(CultureFormattingTest, DateShortFormat) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "d", "C");
    // :d is short date (%x) - should produce something non-empty
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result, kTestTimestamp) << "Date format should transform the timestamp";
}

TEST_F(CultureFormattingTest, DateLongFormat) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "D", "C");
    // :D is long date (%A, %B %d, %Y) - should contain year
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("2024"), std::string::npos)
        << "Long date should contain year 2024. Got: " << result;
    EXPECT_NE(result.find("January") != std::string::npos ||
              result.find("Jan") != std::string::npos, false)
        << "Long date should contain month. Got: " << result;
}

TEST_F(CultureFormattingTest, TimeShortFormat) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "t", "C");
    // :t is short time (%H:%M) - should contain a colon
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find(':'), std::string::npos)
        << "Short time should contain ':'. Got: " << result;
}

TEST_F(CultureFormattingTest, TimeLongFormat) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "T", "C");
    // :T is long time (%H:%M:%S) - should contain two colons
    EXPECT_FALSE(result.empty());
    size_t firstColon = result.find(':');
    ASSERT_NE(firstColon, std::string::npos);
    size_t secondColon = result.find(':', firstColon + 1);
    EXPECT_NE(secondColon, std::string::npos)
        << "Long time should contain HH:MM:SS. Got: " << result;
}

TEST_F(CultureFormattingTest, FullDateShortTime) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "f", "C");
    // :f is full date + short time - should contain year and colon
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("2024"), std::string::npos)
        << "Full date+short time should contain year. Got: " << result;
    EXPECT_NE(result.find(':'), std::string::npos)
        << "Full date+short time should contain time. Got: " << result;
}

TEST_F(CultureFormattingTest, FullDateLongTime) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "F", "C");
    // :F is full date + long time
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("2024"), std::string::npos)
        << "Full date+long time should contain year. Got: " << result;
    size_t firstColon = result.find(':');
    ASSERT_NE(firstColon, std::string::npos);
    size_t secondColon = result.find(':', firstColon + 1);
    EXPECT_NE(secondColon, std::string::npos)
        << "Full date+long time should contain HH:MM:SS. Got: " << result;
}

TEST_F(CultureFormattingTest, DateTimeNonNumericPassthrough) {
    // Non-numeric value should be returned as-is
    std::string result = minta::detail::applyFormat("not-a-timestamp", "d", "C");
    EXPECT_EQ(result, "not-a-timestamp");
}

TEST_F(CultureFormattingTest, DateTimeThroughLogger) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_datetime_test.txt");

    logger.info("Date: {ts:D}", 1705312245);

    logger.flush();
    TestUtils::waitForFileContent("culture_datetime_test.txt");
    std::string content = TestUtils::readLogFile("culture_datetime_test.txt");
    EXPECT_NE(content.find("2024"), std::string::npos)
        << "Long date through logger should contain year. Got: " << content;
}

// ===========================================================================
// 3. Default (no locale) behavior unchanged
// ===========================================================================

TEST_F(CultureFormattingTest, DefaultBehaviorFixedPoint) {
    // Existing .2f spec must still work
    std::string result = minta::detail::applyFormat("3.14159", ".2f", "C");
    EXPECT_EQ(result, "3.14");
}

TEST_F(CultureFormattingTest, DefaultBehaviorCurrency) {
    std::string result = minta::detail::applyFormat("42.5", "C", "C");
    EXPECT_EQ(result, "$42.50");
}

TEST_F(CultureFormattingTest, DefaultBehaviorHex) {
    std::string result = minta::detail::applyFormat("255", "X", "C");
    EXPECT_EQ(result, "FF");
}

TEST_F(CultureFormattingTest, DefaultBehaviorPercentage) {
    std::string result = minta::detail::applyFormat("0.856", "P", "C");
    EXPECT_EQ(result, "85.60%");
}

TEST_F(CultureFormattingTest, DefaultBehaviorZeroPad) {
    std::string result = minta::detail::applyFormat("42", "04", "C");
    EXPECT_EQ(result, "0042");
}

TEST_F(CultureFormattingTest, DefaultBehaviorScientific) {
    std::string result = minta::detail::applyFormat("1500", "e", "C");
    EXPECT_NE(result.find("e+"), std::string::npos)
        << "Scientific notation should contain 'e+'. Got: " << result;
}

TEST_F(CultureFormattingTest, DefaultBehaviorNoSpec) {
    std::string result = minta::detail::applyFormat("hello", "", "C");
    EXPECT_EQ(result, "hello");
}

TEST_F(CultureFormattingTest, DefaultBehaviorThroughLoggerUnchanged) {
    // Logging with format specs that existed before culture support
    // must produce the same results regardless of the new code
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_default_test.txt");

    logger.info("{a:.2f} {b:X} {c:C} {d:P} {e:04}", 3.14159, 255, 9.99, 0.5, 42);

    logger.flush();
    TestUtils::waitForFileContent("culture_default_test.txt");
    std::string content = TestUtils::readLogFile("culture_default_test.txt");
    EXPECT_NE(content.find("3.14 FF $9.99 50.00% 0042"), std::string::npos)
        << "Existing format specs must be unchanged. Got: " << content;
}

// ===========================================================================
// 4. Per-sink locale override
// ===========================================================================

TEST_F(CultureFormattingTest, PerSinkLocaleOverride) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    // Sink 0: default console sink is disabled (false); add two file sinks
    logger.addSink<minta::FileSink>("culture_sink0_test.txt");  // sink 0
    logger.addSink<minta::FileSink>("culture_sink1_test.txt");  // sink 1

    // Logger locale is C (default). Override sink 1 to en_US.
    logger.setSinkLocale(1, "en_US");

    logger.info("Num: {val:n}", 1234567.89);

    logger.flush();
    TestUtils::waitForFileContent("culture_sink0_test.txt");
    TestUtils::waitForFileContent("culture_sink1_test.txt");

    std::string sink0 = TestUtils::readLogFile("culture_sink0_test.txt");
    std::string sink1 = TestUtils::readLogFile("culture_sink1_test.txt");

    // Sink 0 uses logger locale (C) -> no thousand separators
    EXPECT_NE(sink0.find("Num: 1234567.89"), std::string::npos)
        << "Sink 0 (C locale) should have plain number. Got: " << sink0;

    // Sink 1 uses en_US -> should have thousand commas
    EXPECT_NE(sink1.find(','), std::string::npos)
        << "Sink 1 (en_US locale) should have thousand separators. Got: " << sink1;
}

TEST_F(CultureFormattingTest, PerSinkLocaleOverrideDateTime) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_sinkdt0_test.txt");  // sink 0
    logger.addSink<minta::FileSink>("culture_sinkdt1_test.txt");  // sink 1

    // Both use C, but sink 1 is overridden. Even with C, the formatter
    // must produce a re-rendered message via localizedMessage().
    logger.setSinkLocale(1, "C");

    logger.info("Time: {ts:T}", 1705312245);

    logger.flush();
    TestUtils::waitForFileContent("culture_sinkdt0_test.txt");
    TestUtils::waitForFileContent("culture_sinkdt1_test.txt");

    std::string sink0 = TestUtils::readLogFile("culture_sinkdt0_test.txt");
    std::string sink1 = TestUtils::readLogFile("culture_sinkdt1_test.txt");

    // Both should contain a colon (time format)
    EXPECT_NE(sink0.find("Time:"), std::string::npos);
    EXPECT_NE(sink1.find("Time:"), std::string::npos);
}

TEST_F(CultureFormattingTest, PerSinkLocaleRerenderPreservesDuplicateNamedSlots) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_dup_slot_sink0.txt");  // sink 0: en_US at log time
    logger.addSink<minta::FileSink>("culture_dup_slot_sink1.txt");  // sink 1: re-render to C

    logger.setLocale("en_US");
    logger.setSinkLocale(1, "C");

    // Duplicate placeholder name intentionally maps to two positional values.
    logger.info("Dup: {x:n} | {x:n}", 1000.5, 2000.75);

    logger.flush();
    TestUtils::waitForFileContent("culture_dup_slot_sink0.txt");
    TestUtils::waitForFileContent("culture_dup_slot_sink1.txt");

    std::string sink0 = TestUtils::readLogFile("culture_dup_slot_sink0.txt");
    std::string sink1 = TestUtils::readLogFile("culture_dup_slot_sink1.txt");

    EXPECT_NE(sink0.find("Dup: 1,000.5 | 2,000.75"), std::string::npos)
        << "Sink0 must keep logger locale formatting. Got: " << sink0;

    // Re-rendered C-locale message must preserve original slot values,
    // not collapse duplicate name {x} to one value.
    EXPECT_NE(sink1.find("Dup: 1000.5 | 2000.75"), std::string::npos)
        << "Sink1 re-render must preserve duplicate-name slot values. Got: " << sink1;
    EXPECT_EQ(sink1.find("Dup: 1000.5 | 1000.5"), std::string::npos)
        << "Sink1 must not collapse duplicate names to first value. Got: " << sink1;
}

TEST_F(CultureFormattingTest, PerSinkLocaleJsonFormatter) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("culture_json_test.txt");  // sink 0

    logger.setSinkLocale(0, "en_US");

    logger.info("Val: {v:n}", 1234567.89);

    logger.flush();
    TestUtils::waitForFileContent("culture_json_test.txt");
    std::string content = TestUtils::readLogFile("culture_json_test.txt");

    // JSON message should contain the en_US formatted number
    EXPECT_NE(content.find("\"message\":"), std::string::npos);
    // The formatted message inside JSON should have thousand separators
    EXPECT_NE(content.find(','), std::string::npos)
        << "JSON with en_US locale should show thousand separators. Got: " << content;
}

TEST_F(CultureFormattingTest, PerSinkLocaleXmlFormatter) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("culture_xml_test.txt");  // sink 0

    logger.setSinkLocale(0, "en_US");

    logger.info("Val: {v:n}", 1234567.89);

    logger.flush();
    TestUtils::waitForFileContent("culture_xml_test.txt");
    std::string content = TestUtils::readLogFile("culture_xml_test.txt");

    EXPECT_NE(content.find("<message>"), std::string::npos);
    EXPECT_NE(content.find(','), std::string::npos)
        << "XML with en_US locale should show thousand separators. Got: " << content;
}

// ===========================================================================
// 5. Thread-safe locale changes
// ===========================================================================

TEST_F(CultureFormattingTest, ThreadSafeLocaleChange) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_thread_test.txt");

    std::atomic<bool> stop(false);
    const int kIterations = 200;

    // Thread that flips locale back and forth
    std::thread localeChanger([&] {
        for (int i = 0; i < kIterations && !stop; ++i) {
            logger.setLocale(i % 2 == 0 ? "C" : "en_US");
        }
    });

    // Thread that logs concurrently
    std::thread logWriter([&] {
        for (int i = 0; i < kIterations && !stop; ++i) {
            logger.info("Count: {n:n}", i);
        }
    });

    localeChanger.join();
    logWriter.join();

    logger.flush();
    TestUtils::waitForFileContent("culture_thread_test.txt");
    std::string content = TestUtils::readLogFile("culture_thread_test.txt");

    // Verify we got output without crashes — the key goal is no data races
    EXPECT_NE(content.find("Count:"), std::string::npos)
        << "Concurrent locale changes should not crash or lose messages";
}

TEST_F(CultureFormattingTest, GetLocaleMatchesSetLocale) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    EXPECT_EQ(logger.getLocale(), "C");

    logger.setLocale("de_DE");
    EXPECT_EQ(logger.getLocale(), "de_DE");

    logger.setLocale("C");
    EXPECT_EQ(logger.getLocale(), "C");
}

// ===========================================================================
// 6. Invalid locale fallback to C
// ===========================================================================

TEST_F(CultureFormattingTest, InvalidLocaleFallbackNumber) {
    // A completely bogus locale should fall back to C behavior
    std::string result = minta::detail::applyFormat("1234.56", "n", "xx_BOGUS_LOCALE_99");
    // Should not crash, should produce a reasonable result
    EXPECT_FALSE(result.empty());
    // Fallback to classic locale -> no thousand separators, '.' decimal
    EXPECT_EQ(result, "1234.56");
}

TEST_F(CultureFormattingTest, InvalidLocaleFallbackDateTime) {
    std::string result = minta::detail::applyFormat(kTestTimestamp, "D", "xx_BOGUS_LOCALE_99");
    // Should not crash, should produce a date string
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result, kTestTimestamp) << "Should still format as date with fallback locale";
}

TEST_F(CultureFormattingTest, EmptyLocaleFallback) {
    std::string result = minta::detail::applyFormat("1234.56", "n", "");
    EXPECT_EQ(result, "1234.56");
}

TEST_F(CultureFormattingTest, POSIXLocaleFallback) {
    std::string result = minta::detail::applyFormat("1234.56", "n", "POSIX");
    EXPECT_EQ(result, "1234.56");
}

TEST_F(CultureFormattingTest, InvalidLocaleThroughLogger) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_invalid_test.txt");
    logger.setLocale("completely_invalid_locale_xyz");

    logger.info("Num: {val:n}", 9876.54);

    logger.flush();
    TestUtils::waitForFileContent("culture_invalid_test.txt");
    std::string content = TestUtils::readLogFile("culture_invalid_test.txt");
    // Should fall back to C and not crash
    EXPECT_NE(content.find("Num: 9876.54"), std::string::npos)
        << "Invalid locale should fall back to C formatting. Got: " << content;
}

// ===========================================================================
// 7. Edge cases and detail:: function tests
// ===========================================================================

TEST_F(CultureFormattingTest, TryCreateLocaleClassicNames) {
    std::locale loc1 = minta::detail::tryCreateLocale("C");
    EXPECT_EQ(loc1.name(), std::locale::classic().name());

    std::locale loc2 = minta::detail::tryCreateLocale("POSIX");
    EXPECT_EQ(loc2.name(), std::locale::classic().name());

    std::locale loc3 = minta::detail::tryCreateLocale("");
    EXPECT_EQ(loc3.name(), std::locale::classic().name());
}

TEST_F(CultureFormattingTest, TryCreateLocaleInvalid) {
    std::locale loc = minta::detail::tryCreateLocale("not_a_real_locale_abc");
    EXPECT_EQ(loc.name(), std::locale::classic().name());
}

TEST_F(CultureFormattingTest, FormatCultureNumberNegative) {
    std::string result = minta::detail::formatCultureNumber("-1234.56", "C");
    EXPECT_EQ(result, "-1234.56");
}

TEST_F(CultureFormattingTest, FormatCultureNumberZero) {
    std::string result = minta::detail::formatCultureNumber("0", "C");
    EXPECT_EQ(result, "0");
}

TEST_F(CultureFormattingTest, FormatCultureNumberLargePrecision) {
    // Value with many decimal places — precision capped at 15
    std::string result = minta::detail::formatCultureNumber("1.123456789012345678", "C");
    EXPECT_FALSE(result.empty());
}

TEST_F(CultureFormattingTest, FormatCultureDateTimeEpoch) {
    // Epoch: Jan 1, 1970
    std::string result = minta::detail::formatCultureDateTime("0", 'D', "C");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("1970"), std::string::npos)
        << "Epoch should format to 1970. Got: " << result;
}

TEST_F(CultureFormattingTest, FormatCultureDateTimeInvalidSpec) {
    // Invalid spec character should return value as-is
    std::string result = minta::detail::formatCultureDateTime("12345", 'z', "C");
    EXPECT_EQ(result, "12345");
}

TEST_F(CultureFormattingTest, ReformatMessageBasic) {
    // Test detail::reformatMessage directly
    std::vector<std::string> values = {"1234.56"};
    std::string result = minta::detail::reformatMessage("Val: {x:n}", values, "C");
    EXPECT_EQ(result, "Val: 1234.56");
}

TEST_F(CultureFormattingTest, ReformatMessageEscapedBraces) {
    std::vector<std::string> values = {"42"};
    std::string result = minta::detail::reformatMessage("{{literal}} {x:n}", values, "C");
    EXPECT_EQ(result, "{literal} 42");
}

TEST_F(CultureFormattingTest, ReformatMessageMultiplePlaceholders) {
    std::vector<std::string> values = {"100", "200"};
    std::string result = minta::detail::reformatMessage("{a:n} and {b:n}", values, "C");
    EXPECT_EQ(result, "100 and 200");
}

TEST_F(CultureFormattingTest, ReformatMessageWithOperator) {
    std::vector<std::string> values = {"42"};
    std::string result = minta::detail::reformatMessage("Val: {@x:n}", values, "C");
    EXPECT_EQ(result, "Val: 42");
}

TEST_F(CultureFormattingTest, LocalizedMessageSameLocaleNoRerender) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_same_locale_test.txt");
    logger.info("Price: {v:C}", 9.99);

    logger.flush();
    TestUtils::waitForFileContent("culture_same_locale_test.txt");
    std::string content = TestUtils::readLogFile("culture_same_locale_test.txt");
    EXPECT_NE(content.find("Price: $9.99"), std::string::npos)
        << "Same-locale should produce original message. Got: " << content;
}

// ===========================================================================
// 8. R1 review — additional test coverage
// ===========================================================================

TEST_F(CultureFormattingTest, ConcurrentSetSinkLocaleAndLogging) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_conc_sink_test.txt");

    std::atomic<bool> stop(false);
    const int kIterations = 300;

    std::thread sinkLocaleChanger([&] {
        for (int i = 0; i < kIterations && !stop; ++i) {
            logger.setSinkLocale(0, i % 2 == 0 ? "C" : "en_US");
        }
    });

    std::thread logWriter([&] {
        for (int i = 0; i < kIterations && !stop; ++i) {
            logger.info("Val: {n:n}", 1234567.89);
        }
    });

    sinkLocaleChanger.join();
    logWriter.join();

    logger.flush();
    TestUtils::waitForFileContent("culture_conc_sink_test.txt");
    std::string content = TestUtils::readLogFile("culture_conc_sink_test.txt");
    EXPECT_NE(content.find("Val:"), std::string::npos)
        << "Concurrent setSinkLocale + logging must not crash";
}

TEST_F(CultureFormattingTest, LocaleChangeBetweenMessages) {
    if (!isLocaleAvailable("en_US") || !isLocaleAvailable("de_DE")) {
        GTEST_SKIP() << "en_US and de_DE locales required";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_locale_change_test.txt");

    logger.setLocale("en_US");
    logger.info("A: {v:n}", 1234567.89);
    logger.flush();

    logger.setLocale("de_DE");
    logger.info("B: {v:n}", 1234567.89);
    logger.flush();

    TestUtils::waitForFileContent("culture_locale_change_test.txt");
    std::string content = TestUtils::readLogFile("culture_locale_change_test.txt");

    size_t posA = content.find("A: ");
    size_t posB = content.find("B: ");
    ASSERT_NE(posA, std::string::npos);
    ASSERT_NE(posB, std::string::npos);

    std::string lineA = content.substr(posA, content.find('\n', posA) - posA);
    std::string lineB = content.substr(posB, content.find('\n', posB) - posB);

    EXPECT_NE(lineA, lineB)
        << "en_US and de_DE should produce different formatting. A=" << lineA << " B=" << lineB;
}

TEST_F(CultureFormattingTest, MixedSpecifiersSameTemplate) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_mixed_spec_test.txt");
    logger.setLocale("en_US");

    logger.info("{a:n} and {b:.2f}", 1234567.89, 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("culture_mixed_spec_test.txt");
    std::string content = TestUtils::readLogFile("culture_mixed_spec_test.txt");

    EXPECT_NE(content.find(","), std::string::npos)
        << ":n with en_US should have thousand separator. Got: " << content;
    EXPECT_NE(content.find("3.14"), std::string::npos)
        << ":.2f should still work alongside :n. Got: " << content;
}

TEST_F(CultureFormattingTest, VeryLargeNumbersWithN) {
    std::string r1 = minta::detail::applyFormat("1000000000000000", "n", "C");
    EXPECT_FALSE(r1.empty());
    EXPECT_NE(r1.find("1"), std::string::npos);

    std::string r2 = minta::detail::applyFormat("100000000000000000000", "n", "C");
    EXPECT_FALSE(r2.empty());

    if (isLocaleAvailable("en_US")) {
        std::string r3 = minta::detail::applyFormat("1000000000000000", "n", "en_US");
        EXPECT_NE(r3.find(","), std::string::npos)
            << "1e15 with en_US should have separators. Got: " << r3;

        std::string r4 = minta::detail::applyFormat("100000000000000000000", "n", "en_US");
        EXPECT_FALSE(r4.empty())
            << "1e20 should not crash. Got: " << r4;
    }
}

TEST_F(CultureFormattingTest, MultipleDifferentPerSinkLocales) {
    bool hasDE = isLocaleAvailable("de_DE");
    bool hasEN = isLocaleAvailable("en_US");
    bool hasFR = isLocaleAvailable("fr_FR");
    if (!hasDE || !hasEN || !hasFR) {
        GTEST_SKIP() << "de_DE, en_US, and fr_FR locales all required";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_multi0_test.txt");
    logger.addSink<minta::FileSink>("culture_multi1_test.txt");
    logger.addSink<minta::FileSink>("culture_multi2_test.txt");

    logger.setSinkLocale(0, "de_DE");
    logger.setSinkLocale(1, "en_US");
    logger.setSinkLocale(2, "fr_FR");

    logger.info("Num: {v:n}", 1234567.89);
    logger.flush();

    TestUtils::waitForFileContent("culture_multi0_test.txt");
    TestUtils::waitForFileContent("culture_multi1_test.txt");
    TestUtils::waitForFileContent("culture_multi2_test.txt");

    std::string s0 = TestUtils::readLogFile("culture_multi0_test.txt");
    std::string s1 = TestUtils::readLogFile("culture_multi1_test.txt");
    std::string s2 = TestUtils::readLogFile("culture_multi2_test.txt");

    EXPECT_NE(s0.find("Num:"), std::string::npos);
    EXPECT_NE(s1.find("Num:"), std::string::npos);
    EXPECT_NE(s2.find("Num:"), std::string::npos);

    size_t numStart0 = s0.find("Num: ") + 5;
    size_t numStart1 = s1.find("Num: ") + 5;
    std::string num0 = s0.substr(numStart0, s0.find('\n', numStart0) - numStart0);
    std::string num1 = s1.substr(numStart1, s1.find('\n', numStart1) - numStart1);
    EXPECT_NE(num0, num1)
        << "de_DE and en_US should differ. de=" << num0 << " en=" << num1;
}

TEST_F(CultureFormattingTest, NumberFormatScientificNotationInput) {
    // Direct test of the sci-notation precision guard:
    // when the value string itself contains scientific notation,
    // formatCultureNumber detects 'e'/'E' and uses fixed precision.
    std::string result = minta::detail::formatCultureNumber("1.5e+03", "C");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("1500"), std::string::npos)
        << "1.5e+03 should format as fixed-point containing 1500. Got: " << result;
    EXPECT_EQ(result.find('e'), std::string::npos)
        << "Sci-notation should be converted to fixed-point. Got: " << result;
}

TEST_F(CultureFormattingTest, NumberFormatDoubleProducingScientificNotation) {
    // Test with actual double values large enough that toString(double)
    // renders in scientific notation via %.15g, exercising the guard
    // through the full logger pipeline.
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_sci_double_test.txt");

    logger.info("Big: {val:n}", 1.5e+20);

    logger.flush();
    TestUtils::waitForFileContent("culture_sci_double_test.txt");
    std::string content = TestUtils::readLogFile("culture_sci_double_test.txt");

    EXPECT_NE(content.find("Big:"), std::string::npos)
        << "Sci-notation double with :n should produce output. Got: " << content;
    // The sci-notation guard converts to fixed-point, so no 'e+' in output
    size_t bigPos = content.find("Big: ");
    ASSERT_NE(bigPos, std::string::npos);
    std::string numPart = content.substr(bigPos + 5, content.find('\n', bigPos) - bigPos - 5);
    EXPECT_EQ(numPart.find("e+"), std::string::npos)
        << "Sci-notation should be converted to fixed-point by :n. Got: " << numPart;
}

TEST_F(CultureFormattingTest, ResetPerSinkLocaleRevertsToEntryLocale) {
    if (!isLocaleAvailable("en_US")) {
        GTEST_SKIP() << "en_US locale not available on this system";
    }

    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("culture_reset_test.txt");

    logger.setSinkLocale(0, "en_US");
    logger.info("Before: {v:n}", 1234567.89);
    logger.flush();

    logger.setSinkLocale(0, "");
    logger.info("After: {v:n}", 1234567.89);
    logger.flush();

    TestUtils::waitForFileContent("culture_reset_test.txt");
    std::string content = TestUtils::readLogFile("culture_reset_test.txt");

    size_t posBefore = content.find("Before: ");
    size_t posAfter = content.find("After: ");
    ASSERT_NE(posBefore, std::string::npos);
    ASSERT_NE(posAfter, std::string::npos);

    std::string lineBefore = content.substr(posBefore, content.find('\n', posBefore) - posBefore);
    std::string lineAfter = content.substr(posAfter, content.find('\n', posAfter) - posAfter);

    EXPECT_NE(lineBefore.find(","), std::string::npos)
        << "Before reset: en_US should have commas. Got: " << lineBefore;
    EXPECT_EQ(lineAfter.find(","), std::string::npos)
        << "After reset to '': should revert to entry locale (C). Got: " << lineAfter;
}
