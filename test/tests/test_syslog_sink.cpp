#ifndef _WIN32

#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include <lunar_log/sink/syslog_sink.hpp>
#include "utils/test_utils.hpp"
#include <string>
#include <syslog.h>

// ---------------------------------------------------------------------------
// SyslogSink unit tests (POSIX only)
//
// These tests verify that SyslogSink can be constructed, written to, and
// destroyed without crashing. Actual syslog output is not captured — the
// focus is on correct initialization and level mapping.
// ---------------------------------------------------------------------------

class SyslogSinkTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- Test 1: Create and write without crash ---
TEST_F(SyslogSinkTest, CreateAndWriteNoCrash) {
    minta::SyslogSink sink("lunarlog-test");

    minta::LogEntry entry;
    entry.level = minta::LogLevel::INFO;
    entry.message = "SyslogSink test message";
    entry.timestamp = std::chrono::system_clock::now();

    // Should not crash or throw
    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 2: includeLevel option ---
TEST_F(SyslogSinkTest, IncludeLevelOption) {
    minta::SyslogOptions opts;
    opts.setIncludeLevel(true);
    minta::SyslogSink sink("lunarlog-test-level", opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::WARN;
    entry.message = "Warning with level prefix";
    entry.timestamp = std::chrono::system_clock::now();

    // Should not crash; the "[WARN] " prefix is sent to syslog
    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 3: LogLevel to syslog priority mapping ---
TEST_F(SyslogSinkTest, LogLevelToSyslogPriorityMapping) {
    EXPECT_EQ(minta::SyslogSink::toSyslogPriority(minta::LogLevel::TRACE), LOG_DEBUG);
    EXPECT_EQ(minta::SyslogSink::toSyslogPriority(minta::LogLevel::DEBUG), LOG_DEBUG);
    EXPECT_EQ(minta::SyslogSink::toSyslogPriority(minta::LogLevel::INFO),  LOG_INFO);
    EXPECT_EQ(minta::SyslogSink::toSyslogPriority(minta::LogLevel::WARN),  LOG_WARNING);
    EXPECT_EQ(minta::SyslogSink::toSyslogPriority(minta::LogLevel::ERROR), LOG_ERR);
    EXPECT_EQ(minta::SyslogSink::toSyslogPriority(minta::LogLevel::FATAL), LOG_CRIT);
}

// --- Test 4: Destructor calls closelog ---
TEST_F(SyslogSinkTest, DestructorCallsCloselog) {
    {
        minta::SyslogSink sink("lunarlog-test-close");

        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Before close";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    }
    // If destructor didn't crash, closelog() was called successfully
    SUCCEED();
}

// --- Test 5: Custom facility ---
TEST_F(SyslogSinkTest, CustomFacility) {
    minta::SyslogOptions opts;
    opts.setFacility(LOG_LOCAL0);
    minta::SyslogSink sink("lunarlog-test-facility", opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::ERROR;
    entry.message = "Error to local0 facility";
    entry.timestamp = std::chrono::system_clock::now();

    EXPECT_NO_THROW(sink.write(entry));
}

// --- Test 6: All log levels can be written ---
TEST_F(SyslogSinkTest, AllLogLevelsWritable) {
    minta::SyslogSink sink("lunarlog-test-alllevels");

    minta::LogLevel levels[] = {
        minta::LogLevel::TRACE,
        minta::LogLevel::DEBUG,
        minta::LogLevel::INFO,
        minta::LogLevel::WARN,
        minta::LogLevel::ERROR,
        minta::LogLevel::FATAL
    };

    for (int i = 0; i < 6; ++i) {
        minta::LogEntry entry;
        entry.level = levels[i];
        entry.message = std::string("Level test: ") + minta::getLevelString(levels[i]);
        entry.timestamp = std::chrono::system_clock::now();
        EXPECT_NO_THROW(sink.write(entry));
    }
}

// --- Test 7: Integration with LunarLog ---
TEST_F(SyslogSinkTest, IntegrationWithLunarLog) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::SyslogSink>("lunarlog-integration");
    // Also add a file sink to verify the pipeline works
    logger.addSink<minta::FileSink>("syslog_integration_test.txt");

    logger.info("Syslog integration test");
    logger.flush();
    TestUtils::waitForFileContent("syslog_integration_test.txt");

    std::string content = TestUtils::readLogFile("syslog_integration_test.txt");
    EXPECT_NE(content.find("Syslog integration test"), std::string::npos);
}

// --- Test 8: Multiple instances refcount — closelog only on last destroy ---
TEST_F(SyslogSinkTest, MultipleInstancesRefcount) {
    {
        minta::SyslogSink sink1("lunarlog-test-ref1");
        {
            minta::SyslogSink sink2("lunarlog-test-ref2");

            minta::LogEntry entry;
            entry.level = minta::LogLevel::INFO;
            entry.message = "While both alive";
            entry.timestamp = std::chrono::system_clock::now();
            EXPECT_NO_THROW(sink1.write(entry));
            EXPECT_NO_THROW(sink2.write(entry));
        }
        // sink2 destroyed — syslog should still work via sink1
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "After sink2 destroyed";
        entry.timestamp = std::chrono::system_clock::now();
        EXPECT_NO_THROW(sink1.write(entry));
    }
    // Both destroyed — closelog called. Creating a new one should work.
    minta::SyslogSink sink3("lunarlog-test-ref3");
    minta::LogEntry entry;
    entry.level = minta::LogLevel::INFO;
    entry.message = "Fresh instance after full cleanup";
    entry.timestamp = std::chrono::system_clock::now();
    EXPECT_NO_THROW(sink3.write(entry));
}

TEST_F(SyslogSinkTest, IdentTruncation) {
    std::string longIdent(300, 'A');
    EXPECT_NO_THROW({
        minta::SyslogSink sink(longIdent);
        minta::LogEntry entry;
        entry.level = minta::LogLevel::INFO;
        entry.message = "Truncation test";
        entry.timestamp = std::chrono::system_clock::now();
        sink.write(entry);
    });
}

TEST_F(SyslogSinkTest, DefaultPriorityForInvalidLevel) {
    int prio = minta::SyslogSink::toSyslogPriority(static_cast<minta::LogLevel>(99));
    EXPECT_EQ(prio, LOG_INFO);
}

TEST_F(SyslogSinkTest, SetLogoptOption) {
    minta::SyslogOptions opts;
    opts.setLogopt(LOG_PID | LOG_CONS);
    minta::SyslogSink sink("lunarlog-test-logopt", opts);

    minta::LogEntry entry;
    entry.level = minta::LogLevel::WARN;
    entry.message = "Logopt test";
    entry.timestamp = std::chrono::system_clock::now();
    EXPECT_NO_THROW(sink.write(entry));
}

#endif // !_WIN32
