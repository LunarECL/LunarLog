#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>

#include <sys/stat.h>
#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

namespace {

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

void removeMatchingFiles(const std::string& prefix, const std::string& ext) {
    std::vector<std::string> toRemove;
#ifdef _MSC_VER
    struct _finddata_t fi;
    std::string pattern = "./*";
    intptr_t h = _findfirst(pattern.c_str(), &fi);
    if (h != -1) {
        do {
            std::string n = fi.name;
            if (n.size() > prefix.size() && n.compare(0, prefix.size(), prefix) == 0) {
                if (ext.empty() || (n.size() > ext.size() &&
                    n.compare(n.size() - ext.size(), ext.size(), ext) == 0)) {
                    toRemove.push_back(n);
                }
            }
        } while (_findnext(h, &fi) == 0);
        _findclose(h);
    }
#else
    DIR* dir = opendir(".");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            std::string n = ent->d_name;
            if (n.size() > prefix.size() && n.compare(0, prefix.size(), prefix) == 0) {
                if (ext.empty() || (n.size() > ext.size() &&
                    n.compare(n.size() - ext.size(), ext.size(), ext) == 0)) {
                    toRemove.push_back(n);
                }
            }
        }
        closedir(dir);
    }
#endif
    for (size_t i = 0; i < toRemove.size(); ++i) {
        std::remove(toRemove[i].c_str());
    }
}

std::uint64_t fileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<std::uint64_t>(st.st_size);
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void removeIfExists(const std::string& path) {
    std::remove(path.c_str());
}

void removeDirIfExists(const std::string& path) {
#ifdef _MSC_VER
    _rmdir(path.c_str());
#else
    rmdir(path.c_str());
#endif
}

} // anonymous namespace

class RollingFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestUtils::cleanupLogFiles();
        cleanupRollingFiles();
    }
    void TearDown() override {
        TestUtils::cleanupLogFiles();
        cleanupRollingFiles();
    }

    // When adding new test files, add corresponding cleanup entries here
    // to prevent leftover files from affecting other tests.
    void cleanupRollingFiles() {
        removeMatchingFiles("roll_size.", ".log");
        removeMatchingFiles("roll_size.log", "");
        removeMatchingFiles("roll_max.", ".log");
        removeMatchingFiles("roll_max.log", "");
        removeMatchingFiles("roll_name.", ".log");
        removeMatchingFiles("roll_name.log", "");
        removeIfExists("roll_lazy.log");
        removeMatchingFiles("roll_conc.", ".log");
        removeMatchingFiles("roll_conc.log", "");
        removeMatchingFiles("roll_json.", ".log");
        removeMatchingFiles("roll_json.log", "");
        removeMatchingFiles("roll_xml.", ".log");
        removeMatchingFiles("roll_xml.log", "");
        removeMatchingFiles("roll_noext.", "");
        removeMatchingFiles("roll_noext", "");
        removeIfExists("roll_daily.log");
        removeIfExists("roll_hourly.log");
        removeMatchingFiles("roll_custom.", ".log");
        removeMatchingFiles("roll_custom.log", "");
        removeIfExists("roll_dir_test/nested/roll.log");
        removeMatchingFiles("roll_dir_test/nested/roll.", ".log");
        removeDirIfExists("roll_dir_test/nested");
        removeDirIfExists("roll_dir_test");
        removeMatchingFiles("roll_hybrid.", ".log");
        removeMatchingFiles("roll_hybrid.log", "");
        removeMatchingFiles("roll_tmpl_json.", ".log");
        removeMatchingFiles("roll_tmpl_json.log", "");
        removeMatchingFiles("roll_tmpl.", ".log");
        removeMatchingFiles("roll_tmpl.log", "");
        removeMatchingFiles("roll_named_fmt.", ".log");
        removeMatchingFiles("roll_named_fmt.log", "");
        removeMatchingFiles("roll_named.", ".log");
        removeMatchingFiles("roll_named.log", "");
        removeMatchingFiles("roll_total.", ".log");
        removeMatchingFiles("roll_total.log", "");
        removeMatchingFiles("roll_restart.", ".log");
        removeMatchingFiles("roll_restart.log", "");
        removeMatchingFiles("roll_hybrid_sz.", ".log");
        removeMatchingFiles("roll_hybrid_sz.log", "");
        removeMatchingFiles("roll_mid.", ".log");
        removeMatchingFiles("roll_mid.log", "");
        removeMatchingFiles("roll_disc_d.", ".log");
        removeMatchingFiles("roll_disc_d.log", "");
        removeMatchingFiles("roll_disc_dd.", ".log");
        removeMatchingFiles("roll_disc_dd.log", "");
        removeMatchingFiles("roll_disc_hh.", ".log");
        removeMatchingFiles("roll_disc_hh.log", "");
        removeMatchingFiles("roll_disc_inv.", ".log");
        removeMatchingFiles("roll_disc_inv.log", "");
        removeMatchingFiles("roll_disc_hr.", ".log");
        removeMatchingFiles("roll_disc_hr.log", "");
        removeIfExists("roll_same_sec.log");
        removeMatchingFiles("roll_same_sec.", ".log");
        removeMatchingFiles("roll_hhybrid.", ".log");
        removeMatchingFiles("roll_hhybrid.log", "");
        removeMatchingFiles("roll_total_cleanup.", ".log");
        removeMatchingFiles("roll_total_cleanup.log", "");
        removeIfExists("roll_open_fail_dir/roll.log");
        removeMatchingFiles("roll_open_fail_dir/roll.", ".log");
        removeDirIfExists("roll_open_fail_dir");
        removeIfExists("roll_ren_dir/roll.log");
        removeMatchingFiles("roll_ren_dir/roll.", ".log");
        removeDirIfExists("roll_ren_dir");
    }
};

TEST_F(RollingFileTest, SizeBasedRotationTriggers) {
    auto policy = minta::RollingPolicy::size("roll_size.log", 200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 20; ++i) {
        logger.info("Message number {idx} with some padding text", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_size.log"));
    EXPECT_TRUE(fileExists("roll_size.001.log"));
}

TEST_F(RollingFileTest, MaxFilesCleanup) {
    auto policy = minta::RollingPolicy::size("roll_max.log", 100).maxFiles(2);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 40; ++i) {
        logger.info("Padding message number {idx} extra text here", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_max.log"));

    int rolledCount = 0;
    for (int i = 1; i <= 10; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "roll_max.%03d.log", i);
        if (fileExists(buf)) rolledCount++;
    }
    EXPECT_LE(rolledCount, 2);
}

TEST_F(RollingFileTest, FileNamingSize) {
    auto policy = minta::RollingPolicy::size("roll_name.log", 100);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 20; ++i) {
        logger.info("Fill message {idx} with padding content here", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_name.log"));
    EXPECT_TRUE(fileExists("roll_name.001.log"));
}

TEST_F(RollingFileTest, FileNamingNoExtension) {
    auto policy = minta::RollingPolicy::size("roll_noext", 100);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 20; ++i) {
        logger.info("Fill message {idx} with padding content here", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_noext"));
    EXPECT_TRUE(fileExists("roll_noext.001"));
}

TEST_F(RollingFileTest, ConcurrentWrites) {
    auto policy = minta::RollingPolicy::size("roll_conc.log", 500);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < 10; ++i) {
                logger.info("Thread {tid} message {idx}", t, i);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    logger.flush();
    EXPECT_TRUE(fileExists("roll_conc.log"));

    int totalLines = 0;
    auto countLines = [&](const std::string& path) {
        std::string content = readFile(path);
        if (content.empty()) return;
        for (size_t i = 0; i < content.size(); ++i) {
            if (content[i] == '\n') totalLines++;
        }
    };

    countLines("roll_conc.log");
    for (int i = 1; i <= 10; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "roll_conc.%03d.log", i);
        if (fileExists(buf)) countLines(buf);
    }

    EXPECT_EQ(totalLines, 40);
}

TEST_F(RollingFileTest, LazyFileCreation) {
    auto policy = minta::RollingPolicy::size("roll_lazy.log", 1000);

    {
        minta::LunarLog logger(minta::LogLevel::INFO, false);
        auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
        logger.addCustomSink(std::move(sink));

        EXPECT_FALSE(fileExists("roll_lazy.log"));

        logger.info("First message triggers creation");
        logger.flush();
    }

    EXPECT_TRUE(fileExists("roll_lazy.log"));
}

TEST_F(RollingFileTest, DirectoryAutoCreation) {
    auto policy = minta::RollingPolicy::size("roll_dir_test/nested/roll.log", 1000);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    logger.info("Message into nested directory");
    logger.flush();

    EXPECT_TRUE(fileExists("roll_dir_test/nested/roll.log"));
}

TEST_F(RollingFileTest, WorksWithJsonFormatter) {
    auto policy = minta::RollingPolicy::size("roll_json.log", 300);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    sink->useFormatter<minta::JsonFormatter>();
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 10; ++i) {
        logger.info("JSON message {idx}", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_json.log"));
    std::string content = readFile("roll_json.log");
    EXPECT_TRUE(content.find("\"level\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"message\"") != std::string::npos);
}

TEST_F(RollingFileTest, WorksWithXmlFormatter) {
    auto policy = minta::RollingPolicy::size("roll_xml.log", 400);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    sink->useFormatter<minta::XmlFormatter>();
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 10; ++i) {
        logger.info("XML message {idx}", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_xml.log"));
    std::string content = readFile("roll_xml.log");
    EXPECT_TRUE(content.find("<log_entry>") != std::string::npos);
}

TEST_F(RollingFileTest, DailyPolicyCreatesFile) {
    auto policy = minta::RollingPolicy::daily("roll_daily.log");

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    logger.info("Daily log message");
    logger.flush();

    EXPECT_TRUE(fileExists("roll_daily.log"));
    std::string content = readFile("roll_daily.log");
    EXPECT_TRUE(content.find("Daily log message") != std::string::npos);
}

TEST_F(RollingFileTest, HourlyPolicyCreatesFile) {
    auto policy = minta::RollingPolicy::hourly("roll_hourly.log");

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    logger.info("Hourly log message");
    logger.flush();

    EXPECT_TRUE(fileExists("roll_hourly.log"));
    std::string content = readFile("roll_hourly.log");
    EXPECT_TRUE(content.find("Hourly log message") != std::string::npos);
}

TEST_F(RollingFileTest, PolicyBuilderFluent) {
    auto policy = minta::RollingPolicy::daily("roll_custom.log").maxFiles(5).maxSize(1024);

    EXPECT_EQ(policy.basePath(), "roll_custom.log");
    EXPECT_EQ(policy.maxSizeBytes(), 1024u);
    EXPECT_EQ(policy.maxFilesCount(), 5u);
    EXPECT_EQ(policy.rollInterval(), minta::RollInterval::Daily);
}

TEST_F(RollingFileTest, SizeBasedMultipleRotations) {
    auto policy = minta::RollingPolicy::size("roll_size.log", 100);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 50; ++i) {
        logger.info("Rotation test message number {idx} with extra padding", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_size.log"));
    EXPECT_TRUE(fileExists("roll_size.001.log"));
    EXPECT_TRUE(fileExists("roll_size.002.log"));
}

TEST_F(RollingFileTest, AddCustomSinkWithName) {
    auto policy = minta::RollingPolicy::size("roll_custom.log", 10000);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink("rolling", std::move(sink));

    logger.info("Named rolling sink message");
    logger.flush();

    EXPECT_TRUE(fileExists("roll_custom.log"));
    std::string content = readFile("roll_custom.log");
    EXPECT_TRUE(content.find("Named rolling sink message") != std::string::npos);
}

TEST_F(RollingFileTest, HybridSizeAndTimePolicyBuilder) {
    auto policy = minta::RollingPolicy::daily("roll_hybrid.log").maxSize(200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 20; ++i) {
        logger.info("Hybrid mode message {idx} with padding text here", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_hybrid.log"));
}

TEST_F(RollingFileTest, EmptyLoggerNoFile) {
    auto policy = minta::RollingPolicy::size("roll_lazy.log", 1000);

    {
        minta::LunarLog logger(minta::LogLevel::INFO, false);
        auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
        logger.addCustomSink(std::move(sink));
    }

    EXPECT_FALSE(fileExists("roll_lazy.log"));
}

TEST_F(RollingFileTest, TemplateSinkApi) {
    auto policy = minta::RollingPolicy::size("roll_tmpl.log", 200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::RollingFileSink>(policy);

    for (int i = 0; i < 20; ++i) {
        logger.info("Template API message {idx} with padding text", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_tmpl.log"));
    EXPECT_TRUE(fileExists("roll_tmpl.001.log"));
}

TEST_F(RollingFileTest, TemplateSinkApiWithJsonFormatter) {
    auto policy = minta::RollingPolicy::size("roll_tmpl_json.log", 500);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::RollingFileSink, minta::JsonFormatter>(policy);

    for (int i = 0; i < 10; ++i) {
        logger.info("JSON template message {idx}", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_tmpl_json.log"));
    std::string content = readFile("roll_tmpl_json.log");
    EXPECT_TRUE(content.find("\"level\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"message\"") != std::string::npos);
}

TEST_F(RollingFileTest, NamedSinkApi) {
    auto policy = minta::RollingPolicy::size("roll_named.log", 200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::RollingFileSink>(minta::named("app"), policy);

    for (int i = 0; i < 20; ++i) {
        logger.info("Named sink message {idx} padding here", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_named.log"));
    EXPECT_TRUE(fileExists("roll_named.001.log"));
}

TEST_F(RollingFileTest, NamedSinkApiWithFormatter) {
    auto policy = minta::RollingPolicy::size("roll_named_fmt.log", 500);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::RollingFileSink, minta::JsonFormatter>(minta::named("json-roll"), policy);

    for (int i = 0; i < 10; ++i) {
        logger.info("Named JSON message {idx}", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_named_fmt.log"));
    std::string content = readFile("roll_named_fmt.log");
    EXPECT_TRUE(content.find("\"level\"") != std::string::npos);
}

TEST_F(RollingFileTest, PolicyBuilderMaxTotalSize) {
    auto policy = minta::RollingPolicy::size("roll_total.log", 100)
        .maxTotalSize(500)
        .maxFiles(10);

    EXPECT_EQ(policy.maxTotalSizeBytes(), 500u);
    EXPECT_EQ(policy.maxFilesCount(), 10u);
    EXPECT_EQ(policy.maxSizeBytes(), 100u);
}

TEST_F(RollingFileTest, MaxTotalSizeEnforced) {
    auto policy = minta::RollingPolicy::size("roll_total.log", 100).maxTotalSize(250);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 50; ++i) {
        logger.info("Total size test message {idx} with extra padding", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_total.log"));

    std::uint64_t totalRolledSize = 0;
    int rolledCount = 0;
    for (int i = 1; i <= 50; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "roll_total.%03d.log", i);
        if (fileExists(buf)) {
            totalRolledSize += fileSize(buf);
            rolledCount++;
        }
    }

    EXPECT_GT(rolledCount, 0);
    EXPECT_LE(totalRolledSize, 250u);
}

TEST_F(RollingFileTest, DiscoveryOnRestart) {
    auto policy = minta::RollingPolicy::size("roll_restart.log", 100).maxFiles(3);

    // Phase 1: Create some rolled files
    {
        minta::LunarLog logger(minta::LogLevel::INFO, false);
        auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
        logger.addCustomSink(std::move(sink));
        for (int i = 0; i < 30; ++i) {
            logger.info("Phase 1 message {idx} with padding text", i);
        }
        logger.flush();
    }
    // Verify rolled files exist after Phase 1
    {
        int count = 0;
        for (int i = 1; i <= 30; ++i) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "roll_restart.%03d.log", i);
            if (fileExists(buf)) count++;
        }
        EXPECT_GE(count, 1);
        EXPECT_LE(count, 3);
    }

    // Phase 2: "Restart" — new logger, same policy
    {
        minta::LunarLog logger(minta::LogLevel::INFO, false);
        auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
        logger.addCustomSink(std::move(sink));
        for (int i = 0; i < 30; ++i) {
            logger.info("Phase 2 message {idx} with padding text", i);
        }
        logger.flush();
    }
    // maxFiles(3) — verify not more than 3 rolled files remain
    int rolledCount = 0;
    for (int i = 1; i <= 100; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "roll_restart.%03d.log", i);
        if (fileExists(buf)) rolledCount++;
    }
    EXPECT_LE(rolledCount, 3);
}

// Real time-based rotation cannot be unit-tested without waiting for a clock
// period to elapse. DailyPolicyCreatesFile and HourlyPolicyCreatesFile verify
// that time policies initialize correctly. This test verifies that a hybrid
// (time + size) policy triggers rotation on the size threshold, exercising the
// hybrid branch of buildRolledName().
TEST_F(RollingFileTest, HybridPolicySizeRotation) {
    auto policy = minta::RollingPolicy::daily("roll_hybrid_sz.log").maxSize(200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 30; ++i) {
        logger.info("Hybrid size rotation message {idx} padding text", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_hybrid_sz.log"));

    // Hybrid rolled files include date: roll_hybrid_sz.YYYY-MM-DD.NNN.log
    // At least one rotation should have occurred due to size threshold
    int rolledCount = 0;
    std::time_t now = std::time(nullptr);
    std::tm tmBuf;
#ifdef _MSC_VER
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmBuf);

    for (int i = 1; i <= 30; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "roll_hybrid_sz.%s.%03d.log", dateBuf, i);
        if (fileExists(buf)) rolledCount++;
    }
    EXPECT_GE(rolledCount, 1);
}

// ---------------------------------------------------------------------------
// ensureOpen() failure — directory is read-only, file cannot be opened
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, EnsureOpenFailure) {
    std::string dir = "roll_open_fail_dir";
    mkdir(dir.c_str(), 0755);

    // Make directory read-only so file creation fails
    chmod(dir.c_str(), 0555);

    auto policy = minta::RollingPolicy::size(dir + "/roll.log", 1000);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    testing::internal::CaptureStderr();
    // Write triggers ensureOpen → file cannot be opened → stderr
    logger.info("This message triggers ensureOpen failure");
    logger.flush();
    std::string stderrOutput = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(stderrOutput.find("RollingFileSink: failed to open file") != std::string::npos)
        << "stderr: " << stderrOutput;
    EXPECT_FALSE(fileExists(dir + "/roll.log"));

    chmod(dir.c_str(), 0755);
    removeIfExists(dir + "/roll.log");
    removeDirIfExists(dir);
}

// ---------------------------------------------------------------------------
// rename() failure during rotation — directory is read-only when rotating
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, RenameFailureDuringRotation) {
    std::string dir = "roll_ren_dir";
    mkdir(dir.c_str(), 0755);
    // Pre-create the log file so ensureOpen succeeds
    { std::ofstream f(dir + "/roll.log"); f << ""; }

    auto policy = minta::RollingPolicy::size(dir + "/roll.log", 100);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    // First write triggers ensureOpen (succeeds, file exists)
    logger.info("Initial padding message here");
    logger.flush();

    // Make directory read-only — rename needs write permission on directory
    chmod(dir.c_str(), 0555);

    testing::internal::CaptureStderr();
    for (int i = 0; i < 30; ++i) {
        logger.info("Rename failure test message {idx} padding", i);
    }
    logger.flush();
    std::string stderrOutput = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(stderrOutput.find("failed to rename") != std::string::npos)
        << "stderr: " << stderrOutput;

    // Cleanup
    chmod(dir.c_str(), 0755);
    removeMatchingFiles(dir + "/roll.", ".log");
    removeIfExists(dir + "/roll.log");
    removeDirIfExists(dir);
}

// ---------------------------------------------------------------------------
// needsRotation() — same-second time check is skipped
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, SameSecondTimeCheckSkipped) {
    // With an hourly policy (time-based only), many rapid writes within
    // the same second should NOT trigger time-based rotation.
    auto policy = minta::RollingPolicy::hourly("roll_same_sec.log");

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 100; ++i) {
        logger.info("Same second message {idx}", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_same_sec.log"));
    // All 100 messages should be in one file (no time rotation within same second)
    std::string content = readFile("roll_same_sec.log");
    int lineCount = 0;
    for (size_t c = 0; c < content.size(); ++c) {
        if (content[c] == '\n') lineCount++;
    }
    EXPECT_EQ(lineCount, 100);
}

// ---------------------------------------------------------------------------
// isValidRolledMiddle() — pure digits discovered, invalid patterns rejected
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, DiscoveryValidatesPureDigitMiddle) {
    // Pre-create valid rolled files with pure digit middles
    { std::ofstream f("roll_mid.003.log"); f << "x\n"; }
    { std::ofstream f("roll_mid.005.log"); f << "x\n"; }
    // Pre-create files with INVALID middles (should be ignored)
    { std::ofstream f("roll_mid.abc.log"); f << "x\n"; }

    auto policy = minta::RollingPolicy::size("roll_mid.log", 100);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 30; ++i) {
        logger.info("Discovery msg {idx} with padding text here", i);
    }
    logger.flush();

    // Sink discovered 003 and 005 → m_sizeRollIndex = 5
    // First rotation creates 006 (not 001 or 006)
    EXPECT_TRUE(fileExists("roll_mid.006.log"));
    // Invalid file was not discovered and not cleaned up
    EXPECT_TRUE(fileExists("roll_mid.abc.log"));
}

// ---------------------------------------------------------------------------
// isValidRolledMiddle() — YYYY-MM-DD date pattern
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, DiscoveryRecognizesDatePattern) {
    // Pre-create a date-pattern rolled file (date-only middle)
    { std::ofstream f("roll_disc_d.2024-02-17.log"); f << "old data\n"; }

    // daily + size with maxFiles(1) → old date file should be cleaned up
    auto policy = minta::RollingPolicy::daily("roll_disc_d.log").maxSize(100).maxFiles(1);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 50; ++i) {
        logger.info("Date discovery msg {idx} with padding text", i);
    }
    logger.flush();

    // The old 2024-02-17 file was discovered (isValidRolledMiddle accepts
    // date-only patterns) and cleaned up by maxFiles(1)
    EXPECT_FALSE(fileExists("roll_disc_d.2024-02-17.log"));
}

// ---------------------------------------------------------------------------
// isValidRolledMiddle() — YYYY-MM-DD.digits (daily+size hybrid)
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, DiscoveryRecognizesDateDigitsPattern) {
    { std::ofstream f("roll_disc_dd.2024-02-17.001.log"); f << "old\n"; }

    auto policy = minta::RollingPolicy::daily("roll_disc_dd.log").maxSize(100).maxFiles(1);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 50; ++i) {
        logger.info("DD discovery msg {idx} padding text here", i);
    }
    logger.flush();

    // Date.digits pattern accepted → discovered → cleaned up by maxFiles(1)
    EXPECT_FALSE(fileExists("roll_disc_dd.2024-02-17.001.log"));
}

// ---------------------------------------------------------------------------
// isValidRolledMiddle() — YYYY-MM-DD.HH.NNN (hourly+size hybrid)
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, DiscoveryRecognizesHourlyHybridPattern) {
    { std::ofstream f("roll_disc_hh.2024-02-17.14.001.log"); f << "old\n"; }

    auto policy = minta::RollingPolicy::hourly("roll_disc_hh.log").maxSize(100).maxFiles(1);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 50; ++i) {
        logger.info("HH discovery msg {idx} padding text here", i);
    }
    logger.flush();

    // Date.HH.NNN pattern accepted → discovered → cleaned up by maxFiles(1)
    EXPECT_FALSE(fileExists("roll_disc_hh.2024-02-17.14.001.log"));
}

// ---------------------------------------------------------------------------
// isValidRolledMiddle() — invalid patterns are rejected by discovery
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, DiscoveryRejectsInvalidPatterns) {
    // Invalid middle patterns (should NOT be discovered)
    { std::ofstream f("roll_disc_inv.abc.log"); f << "bad\n"; }        // letters
    { std::ofstream f("roll_disc_inv.2024-02.log"); f << "bad\n"; }    // short (7 chars)
    { std::ofstream f("roll_disc_inv.2024-02-17..log"); f << "bad\n"; }// date+dot, empty rest
    // One VALID file to confirm selective discovery
    { std::ofstream f("roll_disc_inv.003.log"); f << "good\n"; }

    auto policy = minta::RollingPolicy::size("roll_disc_inv.log", 100).maxFiles(1);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 50; ++i) {
        logger.info("Invalid discovery msg {idx} padding text", i);
    }
    logger.flush();

    // Invalid files NOT discovered, so NOT cleaned up
    EXPECT_TRUE(fileExists("roll_disc_inv.abc.log"));
    EXPECT_TRUE(fileExists("roll_disc_inv.2024-02.log"));
    EXPECT_TRUE(fileExists("roll_disc_inv.2024-02-17..log"));
}

// ---------------------------------------------------------------------------
// discoverExistingRolledFiles() — hybrid mode index recovery from period.NNN
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, DiscoveryHybridIndexRecovery) {
    // Compute today's date in the format the sink uses
    std::time_t now = std::time(nullptr);
    std::tm tmBuf;
#ifdef _MSC_VER
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char dateBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmBuf);
    std::string period(dateBuf);

    // Pre-create hybrid rolled files for today's period
    { std::ofstream f("roll_disc_hr." + period + ".003.log"); f << "h\n"; }
    { std::ofstream f("roll_disc_hr." + period + ".007.log"); f << "h\n"; }

    auto policy = minta::RollingPolicy::daily("roll_disc_hr.log").maxSize(100);
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 30; ++i) {
        logger.info("Hybrid recovery msg {idx} with padding", i);
    }
    logger.flush();

    // Sink discovered 003 and 007 for today → m_sizeRollIndex = 7
    // First rotation continues at 008
    std::string expectedFile = "roll_disc_hr." + period + ".008.log";
    EXPECT_TRUE(fileExists(expectedFile)) << "Expected: " << expectedFile;
}

// ---------------------------------------------------------------------------
// buildRolledName() — hourly + size hybrid naming
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, HybridHourlyPolicySizeRotation) {
    auto policy = minta::RollingPolicy::hourly("roll_hhybrid.log").maxSize(200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 30; ++i) {
        logger.info("Hourly hybrid rotation message {idx} padding text", i);
    }
    logger.flush();

    EXPECT_TRUE(fileExists("roll_hhybrid.log"));

    // Hourly hybrid rolled files: roll_hhybrid.YYYY-MM-DD.HH.NNN.log
    int rolledCount = 0;
    std::time_t now = std::time(nullptr);
    std::tm tmBuf;
#ifdef _MSC_VER
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char dateBuf[20];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d.%H", &tmBuf);

    for (int i = 1; i <= 30; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "roll_hhybrid.%s.%03d.log", dateBuf, i);
        if (fileExists(buf)) rolledCount++;
    }
    EXPECT_GE(rolledCount, 1);
}

// ---------------------------------------------------------------------------
// cleanup() — maxTotalSize policy removes oldest rolled files
// ---------------------------------------------------------------------------

TEST_F(RollingFileTest, MaxTotalSizeRemovesOldestFiles) {
    // Use a very small maxTotalSize to force aggressive cleanup
    auto policy = minta::RollingPolicy::size("roll_total_cleanup.log", 100)
        .maxTotalSize(200);

    minta::LunarLog logger(minta::LogLevel::INFO, false);
    auto sink = minta::detail::make_unique<minta::RollingFileSink>(policy);
    logger.addCustomSink(std::move(sink));

    for (int i = 0; i < 60; ++i) {
        logger.info("Total cleanup msg {idx} with extra padding", i);
    }
    logger.flush();

    // Count remaining rolled files and their total size
    std::uint64_t totalRolledSize = 0;
    int rolledCount = 0;
    for (int i = 1; i <= 60; ++i) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "roll_total_cleanup.%03d.log", i);
        if (fileExists(buf)) {
            totalRolledSize += fileSize(buf);
            rolledCount++;
        }
    }

    EXPECT_GT(rolledCount, 0);
    EXPECT_LE(totalRolledSize, 200u);
}
