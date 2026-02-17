#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"

class IndexedParametersTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

TEST_F(IndexedParametersTest, BasicIndexedParameters) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_basic.txt");

    logger.info("User {0} from {1}", "Alice", "Seoul");

    logger.flush();
    TestUtils::waitForFileContent("indexed_basic.txt");
    std::string logContent = TestUtils::readLogFile("indexed_basic.txt");

    EXPECT_TRUE(logContent.find("User Alice from Seoul") != std::string::npos);
}

TEST_F(IndexedParametersTest, ReuseIndexedParameter) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_reuse.txt");

    logger.info("{0} sent {1} to {0}", "Alice", "$50");

    logger.flush();
    TestUtils::waitForFileContent("indexed_reuse.txt");
    std::string logContent = TestUtils::readLogFile("indexed_reuse.txt");

    EXPECT_TRUE(logContent.find("Alice sent $50 to Alice") != std::string::npos);
}

TEST_F(IndexedParametersTest, IndexedOutOfRangeIsEmpty) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_outofrange.txt");

    logger.info("Hello {0} {2}", "Alice");

    logger.flush();
    TestUtils::waitForFileContent("indexed_outofrange.txt");
    std::string logContent = TestUtils::readLogFile("indexed_outofrange.txt");

    EXPECT_TRUE(logContent.find("Hello Alice ") != std::string::npos);
    EXPECT_TRUE(logContent.find("Warning: More placeholders than provided values") != std::string::npos);
}

TEST_F(IndexedParametersTest, ReusedIndexedDoesNotTriggerMismatchWarning) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_no_mismatch_warning.txt");

    logger.info("{0} then {0}", "Alice");

    logger.flush();
    TestUtils::waitForFileContent("indexed_no_mismatch_warning.txt");
    std::string logContent = TestUtils::readLogFile("indexed_no_mismatch_warning.txt");

    EXPECT_TRUE(logContent.find("Alice then Alice") != std::string::npos);
    EXPECT_TRUE(logContent.find("More placeholders than provided values") == std::string::npos);
    EXPECT_TRUE(logContent.find("duplicate placeholder name") == std::string::npos);
}

TEST_F(IndexedParametersTest, CoexistNamedAndIndexed) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_mixed.txt");

    logger.info("{name} has {1} items", "Alice", "3");

    logger.flush();
    TestUtils::waitForFileContent("indexed_mixed.txt");
    std::string logContent = TestUtils::readLogFile("indexed_mixed.txt");

    EXPECT_TRUE(logContent.find("Alice has 3 items") != std::string::npos);
}

TEST_F(IndexedParametersTest, NumericPlaceholderDoesNotWarnAsDuplicateName) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_no_dup_warning.txt");

    logger.info("{0} and {0}", "Alice");

    logger.flush();
    TestUtils::waitForFileContent("indexed_no_dup_warning.txt");
    std::string logContent = TestUtils::readLogFile("indexed_no_dup_warning.txt");

    EXPECT_TRUE(logContent.find("Alice and Alice") != std::string::npos);
    EXPECT_TRUE(logContent.find("duplicate placeholder name") == std::string::npos);
}

TEST_F(IndexedParametersTest, IndexedWithFormatAndTransform) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_transform.txt");

    logger.info("Price: {1:.2f} {0|upper}", "alice", 100.123);

    logger.flush();
    TestUtils::waitForFileContent("indexed_transform.txt");
    std::string logContent = TestUtils::readLogFile("indexed_transform.txt");

    EXPECT_TRUE(logContent.find("Price: 100.12 ALICE") != std::string::npos);
}

TEST_F(IndexedParametersTest, MixedNamedAndIndexedReverseOrderUsesStableSlots) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_mixed_reverse.txt");

    logger.info("{1} then {name} then {0}", "A", "B");

    logger.flush();
    TestUtils::waitForFileContent("indexed_mixed_reverse.txt");
    std::string logContent = TestUtils::readLogFile("indexed_mixed_reverse.txt");

    EXPECT_TRUE(logContent.find("B then A then A") != std::string::npos);
}

TEST_F(IndexedParametersTest, IndexedOperatorsAndStructuredOutputAreDeterministic) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("indexed_structured.json");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("indexed_structured.xml");

    logger.info("{@0} {$1} {0} {1}", "123", "hello");

    logger.flush();
    TestUtils::waitForFileContent("indexed_structured.json");
    TestUtils::waitForFileContent("indexed_structured.xml");
    std::string json = TestUtils::readLogFile("indexed_structured.json");
    std::string xml = TestUtils::readLogFile("indexed_structured.xml");

    EXPECT_TRUE(json.find("\"0\":123") != std::string::npos);
    EXPECT_TRUE(json.find("\"1\":\"hello\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"0\":\"123\",\"0\":\"123\"") == std::string::npos);

    EXPECT_TRUE(xml.find("<_0 destructure=\"true\">123</_0>") != std::string::npos);
    EXPECT_TRUE(xml.find("<_1 stringify=\"true\">hello</_1>") != std::string::npos);
    EXPECT_TRUE(xml.find("<_>123</_>") == std::string::npos);
}

TEST_F(IndexedParametersTest, HugeIndexedOverflowIsHandledAsOutOfRange) {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.addSink<minta::FileSink>("indexed_overflow.txt");

    logger.info("overflow {999999999999999999999}", "A");

    logger.flush();
    TestUtils::waitForFileContent("indexed_overflow.txt");
    std::string logContent = TestUtils::readLogFile("indexed_overflow.txt");

    EXPECT_TRUE(logContent.find("overflow ") != std::string::npos);
    EXPECT_TRUE(logContent.find("More placeholders than provided values") != std::string::npos);
}
