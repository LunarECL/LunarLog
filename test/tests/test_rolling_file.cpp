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
