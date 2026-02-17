#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <stdexcept>

class ScopedContextTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// --- Basic scope: keys added and removed ---

TEST_F(ScopedContextTest, BasicScopeAddsAndRemovesKeys) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_basic.txt");

    {
        auto scope = logger.scope({{"requestId", "req-001"}});
        logger.info("Inside scope");
    }
    logger.info("Outside scope");

    logger.flush();
    TestUtils::waitForFileContent("scope_basic.txt");
    std::string content = TestUtils::readLogFile("scope_basic.txt");

    size_t insidePos = content.find("Inside scope");
    ASSERT_NE(insidePos, std::string::npos);
    // Context should appear before the next log line
    size_t outsidePos = content.find("Outside scope");
    ASSERT_NE(outsidePos, std::string::npos);

    std::string insideLine = content.substr(insidePos > 50 ? insidePos - 50 : 0, 120);
    EXPECT_TRUE(insideLine.find("requestId=req-001") != std::string::npos
             || insideLine.find("requestId") != std::string::npos);

    // After scope exit, requestId should not appear near the second message
    std::string outsideLine = content.substr(outsidePos);
    EXPECT_EQ(outsideLine.find("requestId"), std::string::npos);
}

TEST_F(ScopedContextTest, ScopeWithMultipleKeys) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_multi_keys.txt");

    {
        auto scope = logger.scope({{"service", "api"}, {"version", "2.1"}});
        logger.info("Multi-key scope");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_multi_keys.txt");
    std::string content = TestUtils::readLogFile("scope_multi_keys.txt");
    EXPECT_TRUE(content.find("service=api") != std::string::npos);
    EXPECT_TRUE(content.find("version=2.1") != std::string::npos);
}

// --- Nested scopes ---

TEST_F(ScopedContextTest, NestedScopesAddAndRemove) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_nested.txt");

    auto outer = logger.scope({{"service", "api"}});
    {
        auto inner = logger.scope({{"endpoint", "/users"}});
        logger.info("Inner message");
    }
    logger.info("Outer message");

    logger.flush();
    TestUtils::waitForFileContent("scope_nested.txt");
    std::string content = TestUtils::readLogFile("scope_nested.txt");

    size_t innerPos = content.find("Inner message");
    ASSERT_NE(innerPos, std::string::npos);
    std::string innerSection = content.substr(0, content.find("\n", innerPos) + 1);
    EXPECT_TRUE(innerSection.find("service=api") != std::string::npos);
    EXPECT_TRUE(innerSection.find("endpoint=/users") != std::string::npos);

    size_t outerPos = content.find("Outer message");
    ASSERT_NE(outerPos, std::string::npos);
    std::string outerSection = content.substr(outerPos);
    EXPECT_TRUE(outerSection.find("service=api") != std::string::npos);
    EXPECT_EQ(outerSection.find("endpoint"), std::string::npos);
}

TEST_F(ScopedContextTest, NestedScopeInnerShadowsOuter) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("scope_shadow.txt");

    auto outer = logger.scope({{"env", "staging"}});
    {
        auto inner = logger.scope({{"env", "production"}});
        logger.info("Shadowed");
    }
    logger.info("Unshadowed");

    logger.flush();
    TestUtils::waitForFileContent("scope_shadow.txt");
    std::string content = TestUtils::readLogFile("scope_shadow.txt");

    // First entry should have env=production (inner shadows outer)
    size_t shadowedPos = content.find("Shadowed");
    ASSERT_NE(shadowedPos, std::string::npos);
    // Find the line containing "Shadowed"
    size_t lineStart = content.rfind('\n', shadowedPos);
    lineStart = (lineStart == std::string::npos) ? 0 : lineStart;
    size_t lineEnd = content.find('\n', shadowedPos);
    std::string shadowedLine = content.substr(lineStart, lineEnd - lineStart);
    EXPECT_TRUE(shadowedLine.find("production") != std::string::npos);

    // Second entry should have env=staging
    size_t unshadowedPos = content.find("Unshadowed");
    ASSERT_NE(unshadowedPos, std::string::npos);
    lineStart = content.rfind('\n', unshadowedPos);
    lineStart = (lineStart == std::string::npos) ? 0 : lineStart;
    lineEnd = content.find('\n', unshadowedPos);
    std::string unshadowedLine = content.substr(lineStart, lineEnd == std::string::npos ? content.size() - lineStart : lineEnd - lineStart);
    EXPECT_TRUE(unshadowedLine.find("staging") != std::string::npos);
}

// --- Thread-local shadows global setContext ---

TEST_F(ScopedContextTest, ScopedContextShadowsGlobalContext) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("scope_vs_global.txt");

    logger.setContext("env", "global-env");
    logger.setContext("app", "myapp");
    {
        auto scope = logger.scope({{"env", "scoped-env"}});
        logger.info("Scoped shadows global");
    }
    logger.info("Global only");

    logger.flush();
    TestUtils::waitForFileContent("scope_vs_global.txt");
    std::string content = TestUtils::readLogFile("scope_vs_global.txt");

    // First message: env should be scoped-env (thread-local wins), app should be myapp
    size_t scopedPos = content.find("Scoped shadows global");
    ASSERT_NE(scopedPos, std::string::npos);
    size_t lineStart = content.rfind('\n', scopedPos);
    lineStart = (lineStart == std::string::npos) ? 0 : lineStart;
    size_t lineEnd = content.find('\n', scopedPos);
    std::string scopedLine = content.substr(lineStart, lineEnd == std::string::npos ? content.size() - lineStart : lineEnd - lineStart);
    EXPECT_TRUE(scopedLine.find("scoped-env") != std::string::npos);
    EXPECT_TRUE(scopedLine.find("myapp") != std::string::npos);

    // Second message: env should be global-env (scope exited)
    size_t globalPos = content.find("Global only");
    ASSERT_NE(globalPos, std::string::npos);
    lineStart = content.rfind('\n', globalPos);
    lineStart = (lineStart == std::string::npos) ? 0 : lineStart;
    lineEnd = content.find('\n', globalPos);
    std::string globalLine = content.substr(lineStart, lineEnd == std::string::npos ? content.size() - lineStart : lineEnd - lineStart);
    EXPECT_TRUE(globalLine.find("global-env") != std::string::npos);

    logger.clearAllContext();
}

// --- Multi-threaded: independent scopes per thread ---

TEST_F(ScopedContextTest, MultiThreadedIndependentScopes) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_threads.txt");

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    auto worker = [&](const std::string& threadId) {
        auto scope = logger.scope({{"threadId", threadId}});
        ready.fetch_add(1);
        while (!go.load()) { std::this_thread::yield(); }
        logger.info("Thread {tid} logging", threadId);
    };

    std::thread t1(worker, "T1");
    std::thread t2(worker, "T2");
    std::thread t3(worker, "T3");

    while (ready.load() < 3) { std::this_thread::yield(); }
    go.store(true);

    t1.join();
    t2.join();
    t3.join();

    logger.flush();
    TestUtils::waitForFileContent("scope_threads.txt");
    std::string content = TestUtils::readLogFile("scope_threads.txt");

    EXPECT_TRUE(content.find("threadId=T1") != std::string::npos);
    EXPECT_TRUE(content.find("threadId=T2") != std::string::npos);
    EXPECT_TRUE(content.find("threadId=T3") != std::string::npos);
}

TEST_F(ScopedContextTest, ThreadScopeDoesNotLeakToOtherThread) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_no_leak.txt");

    std::atomic<bool> childDone{false};
    std::atomic<bool> parentCheck{false};

    std::thread child([&]() {
        auto scope = logger.scope({{"childKey", "childVal"}});
        logger.info("Child message");
        childDone.store(true);
        while (!parentCheck.load()) { std::this_thread::yield(); }
    });

    while (!childDone.load()) { std::this_thread::yield(); }
    // Parent thread should NOT see child's scope
    logger.info("Parent message");
    parentCheck.store(true);
    child.join();

    logger.flush();
    TestUtils::waitForFileContent("scope_no_leak.txt");
    std::string content = TestUtils::readLogFile("scope_no_leak.txt");

    size_t parentPos = content.find("Parent message");
    ASSERT_NE(parentPos, std::string::npos);
    std::string parentLine = content.substr(parentPos);
    EXPECT_EQ(parentLine.find("childKey"), std::string::npos);
}

// --- Move semantics ---

TEST_F(ScopedContextTest, MoveConstructorTransfersOwnership) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_move_ctor.txt");

    {
        auto original = logger.scope({{"key", "val"}});
        auto moved = std::move(original);
        logger.info("After move ctor");
    }
    logger.info("After both destroyed");

    logger.flush();
    TestUtils::waitForFileContent("scope_move_ctor.txt");
    std::string content = TestUtils::readLogFile("scope_move_ctor.txt");

    size_t afterMovePos = content.find("After move ctor");
    ASSERT_NE(afterMovePos, std::string::npos);
    std::string afterMoveLine = content.substr(
        afterMovePos > 50 ? afterMovePos - 50 : 0,
        120);
    EXPECT_TRUE(afterMoveLine.find("key=val") != std::string::npos);

    size_t afterDestPos = content.find("After both destroyed");
    ASSERT_NE(afterDestPos, std::string::npos);
    std::string afterDestLine = content.substr(afterDestPos);
    EXPECT_EQ(afterDestLine.find("key=val"), std::string::npos);
}

TEST_F(ScopedContextTest, MoveAssignmentTransfersOwnership) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_move_assign.txt");

    {
        auto scope1 = logger.scope({{"first", "1"}});
        auto scope2 = logger.scope({{"second", "2"}});
        // Move scope2 into scope1 — scope1's old frame is popped, scope2's survives
        scope1 = std::move(scope2);
        logger.info("After move assign");
    }
    logger.info("After all destroyed");

    logger.flush();
    TestUtils::waitForFileContent("scope_move_assign.txt");
    std::string content = TestUtils::readLogFile("scope_move_assign.txt");

    // After move-assign: scope1's original frame ("first") was popped.
    // scope2's frame ("second") remains alive (now owned by scope1).
    size_t afterAssignPos = content.find("After move assign");
    ASSERT_NE(afterAssignPos, std::string::npos);
    std::string assignLine = content.substr(
        afterAssignPos > 80 ? afterAssignPos - 80 : 0,
        160);
    EXPECT_TRUE(assignLine.find("second=2") != std::string::npos);
    EXPECT_EQ(assignLine.find("first=1"), std::string::npos);

    size_t afterAllPos = content.find("After all destroyed");
    ASSERT_NE(afterAllPos, std::string::npos);
    std::string afterAllLine = content.substr(afterAllPos);
    EXPECT_EQ(afterAllLine.find("second"), std::string::npos);
    EXPECT_EQ(afterAllLine.find("first"), std::string::npos);
}

// --- Exception safety ---

TEST_F(ScopedContextTest, ScopeCleanedUpDuringStackUnwinding) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_exception.txt");

    try {
        auto scope = logger.scope({{"txn", "tx-999"}});
        logger.info("Before throw");
        throw std::runtime_error("test exception");
    } catch (...) {
        // Scope should have been destroyed during unwinding
    }
    logger.info("After exception");

    logger.flush();
    TestUtils::waitForFileContent("scope_exception.txt");
    std::string content = TestUtils::readLogFile("scope_exception.txt");

    size_t beforePos = content.find("Before throw");
    ASSERT_NE(beforePos, std::string::npos);
    std::string beforeLine = content.substr(
        beforePos > 50 ? beforePos - 50 : 0, 120);
    EXPECT_TRUE(beforeLine.find("txn=tx-999") != std::string::npos);

    size_t afterPos = content.find("After exception");
    ASSERT_NE(afterPos, std::string::npos);
    std::string afterLine = content.substr(afterPos);
    EXPECT_EQ(afterLine.find("txn"), std::string::npos);
}

// --- LogScope::add() ---

TEST_F(ScopedContextTest, AddExpandsScopeAfterCreation) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_add.txt");

    {
        auto scope = logger.scope({{"initial", "yes"}});
        scope.add("added", "later");
        logger.info("With added key");
    }
    logger.info("After add scope");

    logger.flush();
    TestUtils::waitForFileContent("scope_add.txt");
    std::string content = TestUtils::readLogFile("scope_add.txt");

    size_t withPos = content.find("With added key");
    ASSERT_NE(withPos, std::string::npos);
    std::string withLine = content.substr(
        withPos > 60 ? withPos - 60 : 0, 150);
    EXPECT_TRUE(withLine.find("initial=yes") != std::string::npos);
    EXPECT_TRUE(withLine.find("added=later") != std::string::npos);

    size_t afterPos = content.find("After add scope");
    ASSERT_NE(afterPos, std::string::npos);
    std::string afterLine = content.substr(afterPos);
    EXPECT_EQ(afterLine.find("initial"), std::string::npos);
    EXPECT_EQ(afterLine.find("added"), std::string::npos);
}

TEST_F(ScopedContextTest, AddChaining) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_add_chain.txt");

    {
        auto scope = logger.scope({});
        scope.add("a", "1").add("b", "2").add("c", "3");
        logger.info("Chained");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_add_chain.txt");
    std::string content = TestUtils::readLogFile("scope_add_chain.txt");
    EXPECT_TRUE(content.find("a=1") != std::string::npos);
    EXPECT_TRUE(content.find("b=2") != std::string::npos);
    EXPECT_TRUE(content.find("c=3") != std::string::npos);
}

// --- Empty scope ---

TEST_F(ScopedContextTest, EmptyScopeNoKeys) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_empty.txt");

    {
        auto scope = logger.scope({});
        logger.info("Empty scope message");
    }
    logger.info("After empty scope");

    logger.flush();
    TestUtils::waitForFileContent("scope_empty.txt");
    std::string content = TestUtils::readLogFile("scope_empty.txt");
    EXPECT_TRUE(content.find("Empty scope message") != std::string::npos);
    EXPECT_TRUE(content.find("After empty scope") != std::string::npos);
}

// --- Large nesting depth (5+ levels) ---

TEST_F(ScopedContextTest, DeepNesting) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_deep.txt");

    auto s1 = logger.scope({{"level", "1"}});
    auto s2 = logger.scope({{"level", "2"}});
    auto s3 = logger.scope({{"level", "3"}});
    auto s4 = logger.scope({{"level", "4"}});
    auto s5 = logger.scope({{"level", "5"}});
    auto s6 = logger.scope({{"level", "6"}});
    logger.info("Deep nested");

    logger.flush();
    TestUtils::waitForFileContent("scope_deep.txt");
    std::string content = TestUtils::readLogFile("scope_deep.txt");

    // Innermost "level" should win (level=6)
    size_t msgPos = content.find("Deep nested");
    ASSERT_NE(msgPos, std::string::npos);
    std::string line = content.substr(
        msgPos > 60 ? msgPos - 60 : 0, 150);
    EXPECT_TRUE(line.find("level=6") != std::string::npos);
}

TEST_F(ScopedContextTest, DeepNestingDifferentKeys) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_deep_diff.txt");

    auto s1 = logger.scope({{"a", "1"}});
    auto s2 = logger.scope({{"b", "2"}});
    auto s3 = logger.scope({{"c", "3"}});
    auto s4 = logger.scope({{"d", "4"}});
    auto s5 = logger.scope({{"e", "5"}});
    logger.info("All keys");

    logger.flush();
    TestUtils::waitForFileContent("scope_deep_diff.txt");
    std::string content = TestUtils::readLogFile("scope_deep_diff.txt");
    EXPECT_TRUE(content.find("a=1") != std::string::npos);
    EXPECT_TRUE(content.find("b=2") != std::string::npos);
    EXPECT_TRUE(content.find("c=3") != std::string::npos);
    EXPECT_TRUE(content.find("d=4") != std::string::npos);
    EXPECT_TRUE(content.find("e=5") != std::string::npos);
}

// --- Interaction with existing ContextScope (backward compat) ---

TEST_F(ScopedContextTest, ContextScopeStillWorks) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_compat.txt");

    {
        minta::ContextScope oldScope(logger, "oldKey", "oldVal");
        logger.info("Old scope active");
    }
    logger.info("Old scope gone");

    logger.flush();
    TestUtils::waitForFileContent("scope_compat.txt");
    std::string content = TestUtils::readLogFile("scope_compat.txt");

    size_t activePos = content.find("Old scope active");
    ASSERT_NE(activePos, std::string::npos);
    std::string activeLine = content.substr(
        activePos > 50 ? activePos - 50 : 0, 120);
    EXPECT_TRUE(activeLine.find("oldKey=oldVal") != std::string::npos);

    size_t gonePos = content.find("Old scope gone");
    ASSERT_NE(gonePos, std::string::npos);
    std::string goneLine = content.substr(gonePos);
    EXPECT_EQ(goneLine.find("oldKey"), std::string::npos);
}

TEST_F(ScopedContextTest, LogScopeAndContextScopeCoexist) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_coexist.txt");

    minta::ContextScope oldScope(logger, "global", "gval");
    auto newScope = logger.scope({{"scoped", "sval"}});
    logger.info("Both active");

    logger.flush();
    TestUtils::waitForFileContent("scope_coexist.txt");
    std::string content = TestUtils::readLogFile("scope_coexist.txt");
    EXPECT_TRUE(content.find("global=gval") != std::string::npos);
    EXPECT_TRUE(content.find("scoped=sval") != std::string::npos);
}

// --- JSON output includes scoped context ---

TEST_F(ScopedContextTest, JsonOutputIncludesScopedContext) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("scope_json.txt");

    {
        auto scope = logger.scope({{"reqId", "R-42"}, {"userId", "U-7"}});
        logger.info("JSON scoped");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_json.txt");
    std::string content = TestUtils::readLogFile("scope_json.txt");
    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos);
    EXPECT_TRUE(content.find("JSON scoped") != std::string::npos);
    EXPECT_TRUE(content.find("\"context\":{") != std::string::npos
             || content.find("\"context\":") != std::string::npos);
    EXPECT_TRUE(content.find("\"reqId\":\"R-42\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"userId\":\"U-7\"") != std::string::npos);
}

// --- XML output includes scoped context ---

TEST_F(ScopedContextTest, XmlOutputIncludesScopedContext) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("scope_xml.txt");

    {
        auto scope = logger.scope({{"region", "us-east-1"}});
        logger.info("XML scoped");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_xml.txt");
    std::string content = TestUtils::readLogFile("scope_xml.txt");
    EXPECT_TRUE(content.find("XML scoped") != std::string::npos);
    EXPECT_TRUE(content.find("<context>") != std::string::npos);
    EXPECT_TRUE(content.find("<region>us-east-1</region>") != std::string::npos);
}

// --- setContext/clearContext unchanged behavior ---

TEST_F(ScopedContextTest, SetContextClearContextUnchanged) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_globalctx.txt");

    logger.setContext("k1", "v1");
    logger.setContext("k2", "v2");
    logger.info("Both keys");
    logger.clearContext("k1");
    logger.info("Only k2");
    logger.clearAllContext();
    logger.info("No context");

    logger.flush();
    TestUtils::waitForFileContent("scope_globalctx.txt");
    std::string content = TestUtils::readLogFile("scope_globalctx.txt");

    size_t bothPos = content.find("Both keys");
    ASSERT_NE(bothPos, std::string::npos);
    std::string bothLine = content.substr(bothPos > 60 ? bothPos - 60 : 0, 140);
    EXPECT_TRUE(bothLine.find("k1=v1") != std::string::npos);
    EXPECT_TRUE(bothLine.find("k2=v2") != std::string::npos);

    size_t onlyPos = content.find("Only k2");
    ASSERT_NE(onlyPos, std::string::npos);
    std::string onlyLine = content.substr(onlyPos > 60 ? onlyPos - 60 : 0, 140);
    EXPECT_TRUE(onlyLine.find("k2=v2") != std::string::npos);

    size_t noPos = content.find("No context");
    ASSERT_NE(noPos, std::string::npos);
    std::string noLine = content.substr(noPos);
    EXPECT_EQ(noLine.find("k1"), std::string::npos);
    EXPECT_EQ(noLine.find("k2"), std::string::npos);
}

// --- Scope with add() on moved-from does nothing ---

TEST_F(ScopedContextTest, AddOnMovedFromIsNoOp) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_moved_add.txt");

    {
        auto original = logger.scope({{"key", "val"}});
        auto moved = std::move(original);
        original.add("ghost", "nope"); // should be no-op (m_active=false)
        logger.info("After moved add");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_moved_add.txt");
    std::string content = TestUtils::readLogFile("scope_moved_add.txt");
    EXPECT_TRUE(content.find("key=val") != std::string::npos);
    EXPECT_EQ(content.find("ghost"), std::string::npos);
}

// --- Self-move-assignment is safe ---

TEST_F(ScopedContextTest, SelfMoveAssignIsSafe) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_selfmove.txt");

    {
        auto scope = logger.scope({{"safe", "yes"}});
        // Suppress -Wself-move warning
        auto& ref = scope;
        scope = std::move(ref);
        logger.info("After self-move");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_selfmove.txt");
    std::string content = TestUtils::readLogFile("scope_selfmove.txt");
    EXPECT_TRUE(content.find("safe=yes") != std::string::npos);
}

// --- Global + scoped merge with non-overlapping keys ---

TEST_F(ScopedContextTest, GlobalAndScopedMergeNonOverlapping) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_merge.txt");

    logger.setContext("globalKey", "gv");
    {
        auto scope = logger.scope({{"scopedKey", "sv"}});
        logger.info("Merged");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_merge.txt");
    std::string content = TestUtils::readLogFile("scope_merge.txt");
    EXPECT_TRUE(content.find("globalKey=gv") != std::string::npos);
    EXPECT_TRUE(content.find("scopedKey=sv") != std::string::npos);

    logger.clearAllContext();
}

// --- Duplicate key: inner scope shadows outer scope ---

TEST_F(ScopedContextTest, DuplicateKeyInnerShadowsOuter) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("scope_dup_key.txt");

    {
        auto outer = logger.scope({{"env", "outer"}});
        logger.info("Outer only");
        {
            auto inner = logger.scope({{"env", "inner"}});
            logger.info("Inner shadows");
        }
        logger.info("Outer restored");
    }

    logger.flush();
    TestUtils::waitForFileContent("scope_dup_key.txt");
    std::string content = TestUtils::readLogFile("scope_dup_key.txt");

    size_t outerOnly = content.find("Outer only");
    size_t innerShadows = content.find("Inner shadows");
    size_t outerRestored = content.find("Outer restored");
    ASSERT_NE(outerOnly, std::string::npos);
    ASSERT_NE(innerShadows, std::string::npos);
    ASSERT_NE(outerRestored, std::string::npos);

    // Inner scope should shadow: env=inner, not env=outer
    std::string innerLine = content.substr(
        innerShadows > 80 ? innerShadows - 80 : 0, 160);
    EXPECT_TRUE(innerLine.find("env=inner") != std::string::npos);

    // After inner scope exits, env=outer restored
    std::string restoredLine = content.substr(
        outerRestored > 80 ? outerRestored - 80 : 0, 160);
    EXPECT_TRUE(restoredLine.find("env=outer") != std::string::npos);
}

// --- Multiple loggers share thread-wide scope ---

TEST_F(ScopedContextTest, MultipleLoggersShareThreadScope) {
    minta::LunarLog loggerA(minta::LogLevel::INFO, false);
    minta::LunarLog loggerB(minta::LogLevel::INFO, false);
    loggerA.addSink<minta::FileSink>("scope_multi_logger_a.txt");
    loggerB.addSink<minta::FileSink>("scope_multi_logger_b.txt");

    {
        auto scope = loggerA.scope({{"shared", "yes"}});
        loggerA.info("From A");
        loggerB.info("From B");
    }

    loggerA.flush();
    loggerB.flush();
    TestUtils::waitForFileContent("scope_multi_logger_a.txt");
    TestUtils::waitForFileContent("scope_multi_logger_b.txt");

    std::string contentA = TestUtils::readLogFile("scope_multi_logger_a.txt");
    std::string contentB = TestUtils::readLogFile("scope_multi_logger_b.txt");

    // Both loggers see scoped context (thread-wide by design)
    EXPECT_TRUE(contentA.find("shared=yes") != std::string::npos);
    EXPECT_TRUE(contentB.find("shared=yes") != std::string::npos);
}
