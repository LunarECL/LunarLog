#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>
#include <string>
#include <memory>

class SourceLocationMacroTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// ---------------------------------------------------------------------------
// 1. LUNAR_INFO captures correct __FILE__
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, CapturesCorrectFile) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_file.txt");
    logger.setCaptureSourceLocation(true);

    LUNAR_INFO(logger, "File capture test");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_file.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_file.txt");

    // __FILE__ should contain this test file's name
    EXPECT_TRUE(content.find("test_source_location_macros.cpp") != std::string::npos)
        << "Expected file name in output, got: " << content;
}

// ---------------------------------------------------------------------------
// 2. LUNAR_INFO captures correct __LINE__
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, CapturesCorrectLine) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_line.txt");
    logger.setCaptureSourceLocation(true);

    int expectedLine = __LINE__ + 1;
    LUNAR_INFO(logger, "Line capture test");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_line.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_line.txt");

    std::string lineStr = "\"line\":" + std::to_string(expectedLine);
    EXPECT_TRUE(content.find(lineStr) != std::string::npos)
        << "Expected " << lineStr << " in output, got: " << content;
}

// ---------------------------------------------------------------------------
// 3. LUNAR_INFO captures correct __func__
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, CapturesCorrectFunction) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_func.txt");
    logger.setCaptureSourceLocation(true);

    LUNAR_INFO(logger, "Function capture test");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_func.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_func.txt");

    // __func__ inside a gtest TEST_F body contains the test name
    EXPECT_TRUE(content.find("\"function\":\"") != std::string::npos)
        << "Expected function field in output, got: " << content;
    // The function name should be non-empty
    EXPECT_TRUE(content.find("\"function\":\"\"") == std::string::npos)
        << "Function field should not be empty";
}

// ---------------------------------------------------------------------------
// 4. LUNAR_TRACE does NOT log when logger level is INFO (disabled level)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, DisabledLevelNotLogged) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("macro_srcloc_disabled.txt");

    LUNAR_TRACE(logger, "This should not appear");

    // Log something at INFO to ensure the file gets created
    logger.info("Sentinel");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_disabled.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_disabled.txt");

    EXPECT_TRUE(content.find("This should not appear") == std::string::npos)
        << "TRACE should be suppressed when min level is INFO";
    EXPECT_TRUE(content.find("Sentinel") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 5. LUNAR_TRACE disabled level: verify side-effect-free (counter not
//    incremented when level is below minimum)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, DisabledLevelSideEffectFree) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("macro_srcloc_sideeffect.txt");

    int counter = 0;
    LUNAR_TRACE(logger, "Count: {val}", ++counter);

    // counter must NOT have been incremented because TRACE < INFO
    EXPECT_EQ(counter, 0)
        << "Arguments should not be evaluated when level is disabled";
}

// ---------------------------------------------------------------------------
// 6. LUNAR_LOG generic macro works with all levels
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, GenericMacroAllLevels) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("macro_srcloc_generic.txt");

    LUNAR_LOG(logger, minta::LogLevel::TRACE, "generic trace");
    LUNAR_LOG(logger, minta::LogLevel::DEBUG, "generic debug");
    LUNAR_LOG(logger, minta::LogLevel::INFO,  "generic info");
    LUNAR_LOG(logger, minta::LogLevel::WARN,  "generic warn");
    LUNAR_LOG(logger, minta::LogLevel::ERROR, "generic error");
    LUNAR_LOG(logger, minta::LogLevel::FATAL, "generic fatal");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_generic.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_generic.txt");

    EXPECT_TRUE(content.find("generic trace") != std::string::npos);
    EXPECT_TRUE(content.find("generic debug") != std::string::npos);
    EXPECT_TRUE(content.find("generic info")  != std::string::npos);
    EXPECT_TRUE(content.find("generic warn")  != std::string::npos);
    EXPECT_TRUE(content.find("generic error") != std::string::npos);
    EXPECT_TRUE(content.find("generic fatal") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 7. LUNAR_ERROR_EX captures exception type and message
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, ExceptionMacroCapturesTypeAndMessage) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_error_ex.txt");

    std::runtime_error ex("connection refused");
    LUNAR_ERROR_EX(logger, ex, "Operation failed for {user}", "alice");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_error_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_error_ex.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("connection refused") != std::string::npos);
    EXPECT_TRUE(content.find("Operation failed for alice") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 8. LUNAR_FATAL_EX captures nested exception chain
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, ExceptionMacroCapturesNestedChain) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_fatal_ex.txt");

    try {
        try {
            throw std::runtime_error("inner db error");
        } catch (...) {
            std::throw_with_nested(std::logic_error("outer service error"));
        }
    } catch (const std::exception& ex) {
        LUNAR_FATAL_EX(logger, ex, "Request failed");
    }

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_fatal_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_fatal_ex.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("logic_error") != std::string::npos);
    EXPECT_TRUE(content.find("outer service error") != std::string::npos);
    EXPECT_TRUE(content.find("\"chain\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("inner db error") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 9. LUNAR_LOG_NO_MACROS: compile-time check that macros ARE defined when
//    LUNAR_LOG_NO_MACROS is not set (positive case)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, NoMacrosOptOutCompileTimeCheck) {
    // This TU does NOT define LUNAR_LOG_NO_MACROS, so macros must be present
#ifdef LUNAR_INFO
    SUCCEED();
#else
    FAIL() << "LUNAR_INFO should be defined when LUNAR_LOG_NO_MACROS is not set";
#endif

#ifdef LUNAR_LOG
    SUCCEED();
#else
    FAIL() << "LUNAR_LOG should be defined when LUNAR_LOG_NO_MACROS is not set";
#endif

#ifdef LUNAR_ERROR_EX
    SUCCEED();
#else
    FAIL() << "LUNAR_ERROR_EX should be defined when LUNAR_LOG_NO_MACROS is not set";
#endif
}

// ---------------------------------------------------------------------------
// 10. LUNAR_INFO works with pointer logger (dereference syntax)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, WorksWithPointerLogger) {
    auto* logger = new minta::LunarLog(minta::LogLevel::TRACE, false);
    logger->addSink<minta::FileSink>("macro_srcloc_ptr.txt");

    LUNAR_INFO(*logger, "Pointer logger test {val}", 42);

    logger->flush();
    TestUtils::waitForFileContent("macro_srcloc_ptr.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_ptr.txt");

    EXPECT_TRUE(content.find("Pointer logger test 42") != std::string::npos);
    delete logger;
}

// ---------------------------------------------------------------------------
// 11. LUNAR_INFO works with shared_ptr logger
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, WorksWithSharedPtrLogger) {
    auto logger = std::make_shared<minta::LunarLog>(minta::LogLevel::TRACE, false);
    logger->addSink<minta::FileSink>("macro_srcloc_shared.txt");

    LUNAR_INFO(*logger, "Shared pointer test {val}", 99);

    logger->flush();
    TestUtils::waitForFileContent("macro_srcloc_shared.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_shared.txt");

    EXPECT_TRUE(content.find("Shared pointer test 99") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 12. LUNAR_WARN with multiple arguments (message template + args)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, MultipleArguments) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("macro_srcloc_multiarg.txt");

    LUNAR_WARN(logger, "User {user} from {ip} exceeded {limit} requests",
               "alice", "10.0.0.1", 100);

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_multiarg.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_multiarg.txt");

    EXPECT_TRUE(content.find("User alice from 10.0.0.1 exceeded 100 requests") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 13. All 6 level macros log at correct level
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, AllSixLevelsCorrect) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("macro_srcloc_levels.txt");

    LUNAR_TRACE(logger, "trace msg");
    LUNAR_DEBUG(logger, "debug msg");
    LUNAR_INFO(logger,  "info msg");
    LUNAR_WARN(logger,  "warn msg");
    LUNAR_ERROR(logger, "error msg");
    LUNAR_FATAL(logger, "fatal msg");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_levels.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_levels.txt");

    EXPECT_TRUE(content.find("[TRACE]") != std::string::npos);
    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("[INFO]")  != std::string::npos);
    EXPECT_TRUE(content.find("[WARN]")  != std::string::npos);
    EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(content.find("[FATAL]") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 14. Source location shows in output with setCaptureSourceLocation(true)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, SourceLocationInOutput) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_output.txt");
    logger.setCaptureSourceLocation(true);

    LUNAR_WARN(logger, "Check source location");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_output.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_output.txt");

    EXPECT_TRUE(content.find("\"file\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"line\":") != std::string::npos);
    EXPECT_TRUE(content.find("\"function\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("test_source_location_macros.cpp") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 15. LUNAR_TRACE_EX captures exception correctly
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, TraceExCapturesException) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_trace_ex.txt");

    std::runtime_error ex("trace level error");
    LUNAR_TRACE_EX(logger, ex, "Trace exception for {user}", "bob");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_trace_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_trace_ex.txt");

    EXPECT_TRUE(content.find("\"level\":\"TRACE\"") != std::string::npos)
        << "Expected TRACE level, got: " << content;
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("trace level error") != std::string::npos);
    EXPECT_TRUE(content.find("Trace exception for bob") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 16. LUNAR_DEBUG_EX captures exception correctly
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, DebugExCapturesException) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_debug_ex.txt");

    std::runtime_error ex("debug level error");
    LUNAR_DEBUG_EX(logger, ex, "Debug exception for {user}", "carol");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_debug_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_debug_ex.txt");

    EXPECT_TRUE(content.find("\"level\":\"DEBUG\"") != std::string::npos)
        << "Expected DEBUG level, got: " << content;
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("debug level error") != std::string::npos);
    EXPECT_TRUE(content.find("Debug exception for carol") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 17. LUNAR_INFO_EX captures exception correctly
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, InfoExCapturesException) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_info_ex.txt");

    std::runtime_error ex("info level error");
    LUNAR_INFO_EX(logger, ex, "Info exception for {user}", "dave");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_info_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_info_ex.txt");

    EXPECT_TRUE(content.find("\"level\":\"INFO\"") != std::string::npos)
        << "Expected INFO level, got: " << content;
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("info level error") != std::string::npos);
    EXPECT_TRUE(content.find("Info exception for dave") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 18. LUNAR_WARN_EX captures exception correctly
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, WarnExCapturesException) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_warn_ex.txt");

    std::runtime_error ex("warn level error");
    LUNAR_WARN_EX(logger, ex, "Warn exception for {user}", "eve");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_warn_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_warn_ex.txt");

    EXPECT_TRUE(content.find("\"level\":\"WARN\"") != std::string::npos)
        << "Expected WARN level, got: " << content;
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("warn level error") != std::string::npos);
    EXPECT_TRUE(content.find("Warn exception for eve") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 19. Macro works inside if-else without braces (do-while safety)
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, DoWhileSafetyInIfElse) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("macro_srcloc_ifelse.txt");

    bool condition = true;

    // This must compile correctly thanks to do { ... } while(0) wrapping.
    // Without do-while, this would cause a dangling-else compile error.
    if (condition)
        LUNAR_INFO(logger, "condition was true");
    else
        LUNAR_WARN(logger, "condition was false");

    // Also test the else branch
    if (!condition)
        LUNAR_INFO(logger, "negated true");
    else
        LUNAR_WARN(logger, "negated false");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_ifelse.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_ifelse.txt");

    EXPECT_TRUE(content.find("condition was true") != std::string::npos);
    EXPECT_TRUE(content.find("condition was false") == std::string::npos);
    EXPECT_TRUE(content.find("negated false") != std::string::npos);
    EXPECT_TRUE(content.find("negated true") == std::string::npos);
}

// ---------------------------------------------------------------------------
// 20. LUNAR_LOG_EX generic macro with explicit level argument
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, GenericExMacroWithLevel) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("macro_srcloc_generic_ex.txt");

    std::runtime_error ex("generic ex error");
    LUNAR_LOG_EX(logger, minta::LogLevel::WARN, ex, "Generic EX {val}", 123);

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_generic_ex.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_generic_ex.txt");

    EXPECT_TRUE(content.find("\"level\":\"WARN\"") != std::string::npos)
        << "Expected WARN level, got: " << content;
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos)
        << "Expected exception object, got: " << content;
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos)
        << "Expected exception type, got: " << content;
    EXPECT_TRUE(content.find("generic ex error") != std::string::npos)
        << "Expected exception message, got: " << content;
    EXPECT_TRUE(content.find("Generic EX 123") != std::string::npos)
        << "Expected rendered message, got: " << content;
}

// ---------------------------------------------------------------------------
// 21. Disabled-level EX macro: exception argument is NOT evaluated
// ---------------------------------------------------------------------------

TEST_F(SourceLocationMacroTest, DisabledLevelExSideEffectFree) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("macro_srcloc_ex_sideeffect.txt");

    int exCounter = 0;

    // Lambda returns an exception and increments counter as a side effect.
    // When DEBUG < ERROR, the entire if-body (including the ex argument)
    // must be skipped, so exCounter must remain 0.
    auto countingException = [&exCounter]() -> std::runtime_error {
        ++exCounter;
        return std::runtime_error("should not be evaluated");
    };

    LUNAR_DEBUG_EX(logger, countingException(), "disabled EX msg {val}", 42);

    EXPECT_EQ(exCounter, 0)
        << "Exception argument should not be evaluated when level is disabled";

    // Log something at ERROR to create the file
    logger.error("Sentinel");

    logger.flush();
    TestUtils::waitForFileContent("macro_srcloc_ex_sideeffect.txt");
    std::string content = TestUtils::readLogFile("macro_srcloc_ex_sideeffect.txt");

    EXPECT_TRUE(content.find("disabled EX msg") == std::string::npos)
        << "DEBUG_EX should be suppressed when min level is ERROR";
    EXPECT_TRUE(content.find("Sentinel") != std::string::npos);
}
