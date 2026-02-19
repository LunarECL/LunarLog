#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <sys/resource.h>
#include <csignal>
#endif

namespace {

std::string readFileContent(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void removeIfExists(const std::string& path) {
    std::remove(path.c_str());
}

} // anonymous namespace

class FileTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestUtils::cleanupLogFiles();
    }
    void TearDown() override {
        TestUtils::cleanupLogFiles();
    }
};

// ---------------------------------------------------------------------------
// Constructor tests
// ---------------------------------------------------------------------------

TEST_F(FileTransportTest, ConstructorSuccess) {
    std::string path = "ft_basic.log";
    EXPECT_NO_THROW(minta::FileTransport(path, true));
    removeIfExists(path);
}

TEST_F(FileTransportTest, ConstructorThrowsOnBadPath) {
    EXPECT_THROW(
        minta::FileTransport("/nonexistent/deeply/nested/path/test.log"),
        std::runtime_error
    );
}

// ---------------------------------------------------------------------------
// Normal write path tests
// ---------------------------------------------------------------------------

TEST_F(FileTransportTest, WriteWithAutoFlush) {
    std::string path = "ft_autoflush.log";
    {
        minta::FileTransport ft(path, true);
        ft.write("Auto-flushed message");
    }
    std::string content = readFileContent(path);
    EXPECT_TRUE(content.find("Auto-flushed message") != std::string::npos);
}

TEST_F(FileTransportTest, WriteWithoutAutoFlush) {
    std::string path = "ft_noflush.log";
    {
        minta::FileTransport ft(path, false);
        ft.write("No flush message");
    }
    // ofstream destructor flushes the buffer
    std::string content = readFileContent(path);
    EXPECT_TRUE(content.find("No flush message") != std::string::npos);
}

TEST_F(FileTransportTest, MultipleWrites) {
    std::string path = "ft_multi.log";
    {
        minta::FileTransport ft(path, true);
        ft.write("Line one");
        ft.write("Line two");
        ft.write("Line three");
    }
    std::string content = readFileContent(path);
    EXPECT_TRUE(content.find("Line one") != std::string::npos);
    EXPECT_TRUE(content.find("Line two") != std::string::npos);
    EXPECT_TRUE(content.find("Line three") != std::string::npos);
}

TEST_F(FileTransportTest, ConcurrentWrites) {
    std::string path = "ft_concurrent.log";
    {
        minta::FileTransport ft(path, true);
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&ft, t]() {
                for (int i = 0; i < 10; ++i) {
                    ft.write("Thread " + std::to_string(t) + " msg " + std::to_string(i));
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }
    }
    std::string content = readFileContent(path);
    int lineCount = 0;
    for (char c : content) {
        if (c == '\n') lineCount++;
    }
    EXPECT_EQ(lineCount, 40);
}

// ---------------------------------------------------------------------------
// Write failure / recovery path tests (POSIX only)
//
// These tests use RLIMIT_FSIZE to trigger I/O errors in the ofstream
// underlying FileTransport.  A message > stream buffer size forces the
// stream to flush during operator<<, which hits the kernel file-size
// limit and sets failbit.
// ---------------------------------------------------------------------------

#ifndef _MSC_VER

TEST_F(FileTransportTest, WriteFailureTriggersErrorPath) {
    struct rlimit oldLimit;
    ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &oldLimit), 0);

    // Ignore SIGXFSZ so the process is not killed on limit violation
    struct sigaction oldAction, newAction;
    std::memset(&newAction, 0, sizeof(newAction));
    newAction.sa_handler = SIG_IGN;
    sigaction(SIGXFSZ, &newAction, &oldAction);

    std::string path = "ft_error.log";
    // Message larger than typical stream buffer (~8 KB) so operator<<
    // must flush mid-write, hitting the kernel file-size limit.
    std::string longMsg(16384, 'X');

    testing::internal::CaptureStderr();
    {
        minta::FileTransport ft(path, true);
        ft.write("ok");

        // Limit large enough for the stderr capture file (~60 bytes)
        // but far smaller than the 16 KB message
        struct rlimit newLimit;
        newLimit.rlim_cur = 500;
        newLimit.rlim_max = oldLimit.rlim_max;
        setrlimit(RLIMIT_FSIZE, &newLimit);

        ft.write(longMsg);
        ft.write(longMsg);

        setrlimit(RLIMIT_FSIZE, &oldLimit);
    }
    std::fflush(stderr);
    std::string stderrOutput = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(stderrOutput.find("FileTransport: write failed") != std::string::npos)
        << "stderr was: " << stderrOutput;

    sigaction(SIGXFSZ, &oldAction, nullptr);
    removeIfExists(path);
}

TEST_F(FileTransportTest, WriteRecoveryAfterTransientFailure) {
    struct rlimit oldLimit;
    ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &oldLimit), 0);

    struct sigaction oldAction, newAction;
    std::memset(&newAction, 0, sizeof(newAction));
    newAction.sa_handler = SIG_IGN;
    sigaction(SIGXFSZ, &newAction, &oldAction);

    std::string path = "ft_recovery.log";
    std::string longMsg(16384, 'X');

    {
        minta::FileTransport ft(path, true);
        ft.write("First"); // succeeds

        struct rlimit newLimit;
        newLimit.rlim_cur = 20;
        newLimit.rlim_max = oldLimit.rlim_max;
        setrlimit(RLIMIT_FSIZE, &newLimit);

        // Fail the normal path, then fail the error path
        ft.write(longMsg);
        ft.write(longMsg);

        // Restore limit — recovery should now succeed
        setrlimit(RLIMIT_FSIZE, &oldLimit);

        // This write enters the error path: !good() → clear → retry.
        // The short message fits in the stream buffer AND the limit is
        // now removed, so the flush succeeds → recovery.
        ft.write("Recovered");
    }

    std::string content = readFileContent(path);
    EXPECT_TRUE(content.find("First") != std::string::npos);
    EXPECT_TRUE(content.find("Recovered") != std::string::npos);

    sigaction(SIGXFSZ, &oldAction, nullptr);
    removeIfExists(path);
}

TEST_F(FileTransportTest, ErrorReportedFlagSuppressesDuplicateStderr) {
    struct rlimit oldLimit;
    ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &oldLimit), 0);

    struct sigaction oldAction, newAction;
    std::memset(&newAction, 0, sizeof(newAction));
    newAction.sa_handler = SIG_IGN;
    sigaction(SIGXFSZ, &newAction, &oldAction);

    std::string path = "ft_dedup.log";
    std::string longMsg(16384, 'X');

    testing::internal::CaptureStderr();
    {
        minta::FileTransport ft(path, true);
        ft.write("ok");

        struct rlimit newLimit;
        newLimit.rlim_cur = 500;
        newLimit.rlim_max = oldLimit.rlim_max;
        setrlimit(RLIMIT_FSIZE, &newLimit);

        ft.write(longMsg);
        for (int i = 0; i < 5; ++i) {
            ft.write(longMsg);
        }

        setrlimit(RLIMIT_FSIZE, &oldLimit);
    }
    std::fflush(stderr);
    std::string stderrOutput = testing::internal::GetCapturedStderr();

    size_t count = 0;
    size_t pos = 0;
    while ((pos = stderrOutput.find("FileTransport: write failed", pos)) != std::string::npos) {
        ++count;
        pos += 1;
    }
    EXPECT_EQ(count, 1u) << "stderr was: " << stderrOutput;

    sigaction(SIGXFSZ, &oldAction, nullptr);
    removeIfExists(path);
}

#endif // !_MSC_VER
