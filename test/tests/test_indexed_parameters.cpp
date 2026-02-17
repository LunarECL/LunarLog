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
