#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <string>

class OperatorTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- Parsing Tests (human-readable output) ---

TEST_F(OperatorTest, DestructureOperatorBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Hello {@user}", "Alice");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Hello Alice") != std::string::npos);
}

TEST_F(OperatorTest, StringifyOperatorBasic) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Hello {$user}", "Bob");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Hello Bob") != std::string::npos);
}

TEST_F(OperatorTest, DestructureWithFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Price: {@amount:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Price: 3.14") != std::string::npos);
    EXPECT_EQ(logContent.find("Price: 3.14159"), std::string::npos);
}

TEST_F(OperatorTest, StringifyWithFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Hex: {$val:X}", 255);

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Hex: FF") != std::string::npos);
}

TEST_F(OperatorTest, MixedOperatorsAndPlain) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("{@a} {$b} {c}", "one", "two", "three");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("one two three") != std::string::npos);
}

TEST_F(OperatorTest, NoOperatorUnchanged) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {x}", "hello");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: hello") != std::string::npos);
}

// --- Edge Cases ---

TEST_F(OperatorTest, DestructureEmptyNameIsLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {@}", "value");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: {@}") != std::string::npos);
}

TEST_F(OperatorTest, StringifyEmptyNameIsLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {$}", "value");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: {$}") != std::string::npos);
}

TEST_F(OperatorTest, DoubleAtSignIsLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {@@val}", "test");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: {@@val}") != std::string::npos);
}

// --- JSON Formatter Tests ---

TEST_F(OperatorTest, JsonDestructureNumericInt) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Count: {@count}", 42);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"properties\":{") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"count\":42") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureNumericFloat) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Rate: {@rate}", 3.14);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"properties\":{") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"rate\":3.14") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureBoolTrue) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Flag: {@flag}", true);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"flag\":true") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureBoolFalse) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Flag: {@flag}", false);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"flag\":false") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureStringFallback) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("User: {@user}", "Alice");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"user\":\"Alice\"") != std::string::npos);
}

TEST_F(OperatorTest, JsonStringifyAlwaysString) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Count: {$count}", 42);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"count\":\"42\"") != std::string::npos);
}

TEST_F(OperatorTest, JsonNoOperatorAlwaysString) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Count: {count}", 42);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"count\":\"42\"") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureNegativeNumber) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Temp: {@temp}", -10);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"temp\":-10") != std::string::npos);
}

TEST_F(OperatorTest, JsonMultipleProperties) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("{@count} by {$user} ok {status}", 5, "Alice", "active");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"properties\":{") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"count\":5") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"user\":\"Alice\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"status\":\"active\"") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureWithFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Price: {@amount:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"Price: 3.14\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"properties\":{") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"amount\":3.14159") != std::string::npos);
}

// --- XML Formatter Tests ---

TEST_F(OperatorTest, XmlDestructureAttribute) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("operator_xml_test.txt");

    logger.info("User: {@user}", "Alice");

    logger.flush();
    TestUtils::waitForFileContent("operator_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_xml_test.txt");

    EXPECT_TRUE(logContent.find("<properties>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<user destructure=\"true\">Alice</user>") != std::string::npos);
    EXPECT_TRUE(logContent.find("</properties>") != std::string::npos);
}

TEST_F(OperatorTest, XmlStringifyAttribute) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("operator_xml_test.txt");

    logger.info("User: {$user}", "Bob");

    logger.flush();
    TestUtils::waitForFileContent("operator_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_xml_test.txt");

    EXPECT_TRUE(logContent.find("<properties>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<user stringify=\"true\">Bob</user>") != std::string::npos);
    EXPECT_TRUE(logContent.find("destructure") == std::string::npos);
}

TEST_F(OperatorTest, XmlNoOperatorNoAttribute) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("operator_xml_test.txt");

    logger.info("User: {user}", "Carol");

    logger.flush();
    TestUtils::waitForFileContent("operator_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_xml_test.txt");

    EXPECT_TRUE(logContent.find("<properties>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<user>Carol</user>") != std::string::npos);
    EXPECT_TRUE(logContent.find("destructure") == std::string::npos);
}

TEST_F(OperatorTest, XmlMultiplePropertiesMixed) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("operator_xml_test.txt");

    logger.info("{@a} {$b} {c}", "1", "2", "3");

    logger.flush();
    TestUtils::waitForFileContent("operator_xml_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_xml_test.txt");

    EXPECT_TRUE(logContent.find("<a destructure=\"true\">1</a>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<b stringify=\"true\">2</b>") != std::string::npos);
    EXPECT_TRUE(logContent.find("<c>3</c>") != std::string::npos);
}

// --- Human-Readable Output Unchanged ---

TEST_F(OperatorTest, HumanReadableUnchangedWithOperators) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("{@user} bought {$count} items for {price:C}", "Alice", 3, 29.99);

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Alice bought 3 items for $29.99") != std::string::npos);
}

TEST_F(OperatorTest, DestructureCurrencyFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Cost: {@price:C}", 19.99);

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Cost: $19.99") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureEmptyString) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Val: {@v}", "");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"v\":\"\"") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureZero) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Val: {@v}", 0);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"v\":0") != std::string::npos);
}

// --- L2: Stringify forces string in JSON even for bool/numeric values ---

TEST_F(OperatorTest, JsonStringifyBoolTrue) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Flag: {$flag}", true);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"flag\":\"true\"") != std::string::npos);
}

TEST_F(OperatorTest, JsonStringifyNumeric) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Rate: {$rate}", 3.14);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"rate\":\"3.14\"") != std::string::npos);
}

// --- L2: Destructure coerces string args that look like bool/number ---

TEST_F(OperatorTest, JsonDestructureStringBoolCoercion) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Tag: {@tag}", "true");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"tag\":true") != std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureStringNumericCoercion) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Tag: {@tag}", "42");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"tag\":42") != std::string::npos);
}

// --- H1: toJsonNativeValue must re-serialize non-JSON numeric forms ---

TEST_F(OperatorTest, JsonDestructurePlusPrefix) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Val: {@v}", "+42");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"v\":42") != std::string::npos);
    EXPECT_EQ(logContent.find("\"v\":\"+42\""), std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureLeadingSpace) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Val: {@v}", " 42");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"v\":42") != std::string::npos);
    EXPECT_EQ(logContent.find("\"v\":\" 42\""), std::string::npos);
}

TEST_F(OperatorTest, JsonDestructureHexLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Val: {@v}", "0x1A");

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"v\":26") != std::string::npos);
    EXPECT_EQ(logContent.find("\"v\":\"0x1A\""), std::string::npos);
}

// --- M1: operator + non-identifier name treated as literal ---

TEST_F(OperatorTest, DestructureSpaceNameIsLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {@ }", "value");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: {@ }") != std::string::npos);
}

// --- M2: JsonDestructureWithFormatSpec raw value assertion ---

TEST_F(OperatorTest, JsonDestructureWithFormatSpecRawValue) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Price: {@amount:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"Price: 3.14\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"amount\":3.14159") != std::string::npos);
}

// --- L1: double-operator combinations render as literal ---

TEST_F(OperatorTest, DoubleDollarIsLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {$$val}", "test");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: {$$val}") != std::string::npos);
}

TEST_F(OperatorTest, DollarAtIsLiteral) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("operator_test.txt");

    logger.info("Val: {$@val}", "test");

    logger.flush();
    TestUtils::waitForFileContent("operator_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_test.txt");
    EXPECT_TRUE(logContent.find("Val: {$@val}") != std::string::npos);
}

// --- L1: operator with namespaced placeholder ---

TEST_F(OperatorTest, JsonDestructureNamespacedPlaceholder) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Rate: {@ns:key:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"message\":\"Rate: 3.14\"") != std::string::npos);
    EXPECT_TRUE(logContent.find("\"ns:key\":3.14159") != std::string::npos);
}

// --- L2: underscore-prefixed operator name ---

TEST_F(OperatorTest, DestructureUnderscorePrefixedName) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operator_json_test.txt");

    logger.info("Val: {@_private}", 42);

    logger.flush();
    TestUtils::waitForFileContent("operator_json_test.txt");
    std::string logContent = TestUtils::readLogFile("operator_json_test.txt");

    EXPECT_TRUE(logContent.find("\"_private\":42") != std::string::npos);
}
