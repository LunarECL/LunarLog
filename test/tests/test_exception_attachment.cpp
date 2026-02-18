#include <gtest/gtest.h>
#include "lunar_log.hpp"
#include "utils/test_utils.hpp"
#include <stdexcept>
#include <string>
#include <regex>

class ExceptionAttachmentTest : public ::testing::Test {
protected:
    void SetUp() override { TestUtils::cleanupLogFiles(); }
    void TearDown() override { TestUtils::cleanupLogFiles(); }
};

// ---------------------------------------------------------------------------
// Exception info extraction (unit tests for detail functions)
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, DemangleStdRuntimeError) {
    std::runtime_error ex("test error");
    std::string typeName = minta::detail::getExceptionTypeName(ex);
    EXPECT_TRUE(typeName.find("runtime_error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, DemangleStdLogicError) {
    std::logic_error ex("logic fail");
    std::string typeName = minta::detail::getExceptionTypeName(ex);
    EXPECT_TRUE(typeName.find("logic_error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, DemangleStdInvalidArgument) {
    std::invalid_argument ex("bad arg");
    std::string typeName = minta::detail::getExceptionTypeName(ex);
    EXPECT_TRUE(typeName.find("invalid_argument") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, ExtractBasicExceptionInfo) {
    std::runtime_error ex("connection refused");
    auto info = minta::detail::extractExceptionInfo(ex);
    EXPECT_TRUE(info.type.find("runtime_error") != std::string::npos);
    EXPECT_EQ(info.message, "connection refused");
    EXPECT_TRUE(info.chain.empty());
}

TEST_F(ExceptionAttachmentTest, ExtractNestedExceptionInfo) {
    try {
        try {
            throw std::runtime_error("inner error");
        } catch (...) {
            std::throw_with_nested(std::logic_error("outer error"));
        }
    } catch (const std::exception& ex) {
        auto info = minta::detail::extractExceptionInfo(ex);
        EXPECT_TRUE(info.type.find("logic_error") != std::string::npos);
        EXPECT_EQ(info.message, "outer error");
        EXPECT_FALSE(info.chain.empty());
        EXPECT_TRUE(info.chain.find("runtime_error") != std::string::npos);
        EXPECT_TRUE(info.chain.find("inner error") != std::string::npos);
    }
}

TEST_F(ExceptionAttachmentTest, ExtractDoublyNestedExceptionInfo) {
    try {
        try {
            try {
                throw std::runtime_error("level 0");
            } catch (...) {
                std::throw_with_nested(std::logic_error("level 1"));
            }
        } catch (...) {
            std::throw_with_nested(std::invalid_argument("level 2"));
        }
    } catch (const std::exception& ex) {
        auto info = minta::detail::extractExceptionInfo(ex);
        EXPECT_TRUE(info.type.find("invalid_argument") != std::string::npos);
        EXPECT_EQ(info.message, "level 2");
        EXPECT_TRUE(info.chain.find("logic_error") != std::string::npos);
        EXPECT_TRUE(info.chain.find("level 1") != std::string::npos);
        EXPECT_TRUE(info.chain.find("runtime_error") != std::string::npos);
        EXPECT_TRUE(info.chain.find("level 0") != std::string::npos);
    }
}

TEST_F(ExceptionAttachmentTest, DemangleNullptrFallback) {
    std::string result = minta::detail::demangleTypeName(nullptr);
    EXPECT_EQ(result, "unknown");
}

// ---------------------------------------------------------------------------
// Basic exception attachment via logger (human-readable output)
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, BasicExceptionWithMessage) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_basic.txt");

    std::runtime_error ex("connection refused");
    logger.error(ex, "Operation failed for user {name}", "john");

    logger.flush();
    TestUtils::waitForFileContent("ex_basic.txt");
    std::string content = TestUtils::readLogFile("ex_basic.txt");

    EXPECT_TRUE(content.find("Operation failed for user john") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("connection refused") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, ExceptionOnlyNoMessage) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_no_msg.txt");

    std::runtime_error ex("connection refused");
    logger.error(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_no_msg.txt");
    std::string content = TestUtils::readLogFile("ex_no_msg.txt");

    EXPECT_TRUE(content.find("connection refused") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
}

// ---------------------------------------------------------------------------
// All log levels with exception
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, TraceLevel) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("ex_trace.txt");

    std::runtime_error ex("trace error");
    logger.trace(ex, "Trace with exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_trace.txt");
    std::string content = TestUtils::readLogFile("ex_trace.txt");

    EXPECT_TRUE(content.find("[TRACE]") != std::string::npos);
    EXPECT_TRUE(content.find("Trace with exception") != std::string::npos);
    EXPECT_TRUE(content.find("trace error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, DebugLevel) {
    minta::LunarLog logger(minta::LogLevel::DEBUG, false);
    logger.addSink<minta::FileSink>("ex_debug.txt");

    std::runtime_error ex("debug error");
    logger.debug(ex, "Debug with exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_debug.txt");
    std::string content = TestUtils::readLogFile("ex_debug.txt");

    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("debug error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, InfoLevel) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("ex_info.txt");

    std::runtime_error ex("info error");
    logger.info(ex, "Info with exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_info.txt");
    std::string content = TestUtils::readLogFile("ex_info.txt");

    EXPECT_TRUE(content.find("[INFO]") != std::string::npos);
    EXPECT_TRUE(content.find("info error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, WarnLevel) {
    minta::LunarLog logger(minta::LogLevel::WARN, false);
    logger.addSink<minta::FileSink>("ex_warn.txt");

    std::runtime_error ex("warn error");
    logger.warn(ex, "Warn with exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_warn.txt");
    std::string content = TestUtils::readLogFile("ex_warn.txt");

    EXPECT_TRUE(content.find("[WARN]") != std::string::npos);
    EXPECT_TRUE(content.find("warn error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, ErrorLevel) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_error.txt");

    std::runtime_error ex("error error");
    logger.error(ex, "Error with exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_error.txt");
    std::string content = TestUtils::readLogFile("ex_error.txt");

    EXPECT_TRUE(content.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(content.find("error error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, FatalLevel) {
    minta::LunarLog logger(minta::LogLevel::FATAL, false);
    logger.addSink<minta::FileSink>("ex_fatal.txt");

    std::runtime_error ex("fatal error");
    logger.fatal(ex, "Fatal with exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_fatal.txt");
    std::string content = TestUtils::readLogFile("ex_fatal.txt");

    EXPECT_TRUE(content.find("[FATAL]") != std::string::npos);
    EXPECT_TRUE(content.find("fatal error") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, TraceLevelNoMessage) {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);
    logger.addSink<minta::FileSink>("ex_trace_nomsg.txt");

    std::runtime_error ex("trace only");
    logger.trace(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_trace_nomsg.txt");
    std::string content = TestUtils::readLogFile("ex_trace_nomsg.txt");

    EXPECT_TRUE(content.find("[TRACE]") != std::string::npos);
    EXPECT_TRUE(content.find("trace only") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, DebugLevelNoMessage) {
    minta::LunarLog logger(minta::LogLevel::DEBUG, false);
    logger.addSink<minta::FileSink>("ex_debug_nomsg.txt");

    std::runtime_error ex("debug only");
    logger.debug(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_debug_nomsg.txt");
    std::string content = TestUtils::readLogFile("ex_debug_nomsg.txt");

    EXPECT_TRUE(content.find("[DEBUG]") != std::string::npos);
    EXPECT_TRUE(content.find("debug only") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, InfoLevelNoMessage) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("ex_info_nomsg.txt");

    std::runtime_error ex("info only");
    logger.info(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_info_nomsg.txt");
    std::string content = TestUtils::readLogFile("ex_info_nomsg.txt");

    EXPECT_TRUE(content.find("info only") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, WarnLevelNoMessage) {
    minta::LunarLog logger(minta::LogLevel::WARN, false);
    logger.addSink<minta::FileSink>("ex_warn_nomsg.txt");

    std::runtime_error ex("warn only");
    logger.warn(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_warn_nomsg.txt");
    std::string content = TestUtils::readLogFile("ex_warn_nomsg.txt");

    EXPECT_TRUE(content.find("warn only") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, FatalLevelNoMessage) {
    minta::LunarLog logger(minta::LogLevel::FATAL, false);
    logger.addSink<minta::FileSink>("ex_fatal_nomsg.txt");

    std::runtime_error ex("fatal only");
    logger.fatal(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_fatal_nomsg.txt");
    std::string content = TestUtils::readLogFile("ex_fatal_nomsg.txt");

    EXPECT_TRUE(content.find("fatal only") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Nested exception rendering in human-readable format
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, NestedExceptionHumanReadable) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_nested_hr.txt");

    try {
        try {
            throw std::runtime_error("inner failure");
        } catch (...) {
            std::throw_with_nested(std::logic_error("outer failure"));
        }
    } catch (const std::exception& ex) {
        logger.error(ex, "Request failed");
    }

    logger.flush();
    TestUtils::waitForFileContent("ex_nested_hr.txt");
    std::string content = TestUtils::readLogFile("ex_nested_hr.txt");

    EXPECT_TRUE(content.find("Request failed") != std::string::npos);
    EXPECT_TRUE(content.find("logic_error") != std::string::npos);
    EXPECT_TRUE(content.find("outer failure") != std::string::npos);
    EXPECT_TRUE(content.find("---") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("inner failure") != std::string::npos);
}

// ---------------------------------------------------------------------------
// JSON formatter with exception
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, JsonFormatterBasicException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_json.txt");

    std::runtime_error ex("connection refused");
    logger.error(ex, "Operation failed for user {name}", "john");

    logger.flush();
    TestUtils::waitForFileContent("ex_json.txt");
    std::string content = TestUtils::readLogFile("ex_json.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"type\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("\"message\":\"connection refused\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"name\":\"john\"") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, JsonFormatterNoException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_json_none.txt");

    logger.error("Normal error {code}", "500");

    logger.flush();
    TestUtils::waitForFileContent("ex_json_none.txt");
    std::string content = TestUtils::readLogFile("ex_json_none.txt");

    EXPECT_TRUE(content.find("\"exception\"") == std::string::npos);
}

TEST_F(ExceptionAttachmentTest, JsonFormatterExceptionNoMessage) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_json_nomsg.txt");

    std::runtime_error ex("db timeout");
    logger.error(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_json_nomsg.txt");
    std::string content = TestUtils::readLogFile("ex_json_nomsg.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"message\":\"db timeout\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Compact JSON formatter with exception (@x field)
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, CompactJsonBasicException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("ex_cjson.txt");

    std::runtime_error ex("connection refused");
    logger.error(ex, "Operation failed for user {name}", "john");

    logger.flush();
    TestUtils::waitForFileContent("ex_cjson.txt");
    std::string content = TestUtils::readLogFile("ex_cjson.txt");

    EXPECT_TRUE(content.find("\"@x\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("connection refused") != std::string::npos);
    EXPECT_TRUE(content.find("\"@l\":\"ERR\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"name\":\"john\"") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, CompactJsonNoException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("ex_cjson_none.txt");

    logger.error("Normal error");

    logger.flush();
    TestUtils::waitForFileContent("ex_cjson_none.txt");
    std::string content = TestUtils::readLogFile("ex_cjson_none.txt");

    EXPECT_TRUE(content.find("\"@x\"") == std::string::npos);
}

TEST_F(ExceptionAttachmentTest, CompactJsonNestedException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("ex_cjson_nested.txt");

    try {
        try {
            throw std::runtime_error("db query failed");
        } catch (...) {
            std::throw_with_nested(std::logic_error("service layer error"));
        }
    } catch (const std::exception& ex) {
        logger.error(ex, "Request failed");
    }

    logger.flush();
    TestUtils::waitForFileContent("ex_cjson_nested.txt");
    std::string content = TestUtils::readLogFile("ex_cjson_nested.txt");

    EXPECT_TRUE(content.find("\"@x\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("logic_error") != std::string::npos);
    EXPECT_TRUE(content.find("service layer error") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("db query failed") != std::string::npos);
}

// ---------------------------------------------------------------------------
// XML formatter with exception
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, XmlFormatterBasicException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("ex_xml.txt");

    std::runtime_error ex("connection refused");
    logger.error(ex, "Operation failed for user {name}", "john");

    logger.flush();
    TestUtils::waitForFileContent("ex_xml.txt");
    std::string content = TestUtils::readLogFile("ex_xml.txt");

    EXPECT_TRUE(content.find("<exception type=\"") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("connection refused") != std::string::npos);
    EXPECT_TRUE(content.find("</exception>") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, XmlFormatterNoException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("ex_xml_none.txt");

    logger.error("Normal error");

    logger.flush();
    TestUtils::waitForFileContent("ex_xml_none.txt");
    std::string content = TestUtils::readLogFile("ex_xml_none.txt");

    EXPECT_TRUE(content.find("<exception") == std::string::npos);
}

TEST_F(ExceptionAttachmentTest, XmlFormatterNestedException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::XmlFormatter>("ex_xml_nested.txt");

    try {
        try {
            throw std::runtime_error("inner");
        } catch (...) {
            std::throw_with_nested(std::logic_error("outer"));
        }
    } catch (const std::exception& ex) {
        logger.error(ex, "Nested failure");
    }

    logger.flush();
    TestUtils::waitForFileContent("ex_xml_nested.txt");
    std::string content = TestUtils::readLogFile("ex_xml_nested.txt");

    EXPECT_TRUE(content.find("<exception type=\"") != std::string::npos);
    EXPECT_TRUE(content.find("<chain>") != std::string::npos);
    EXPECT_TRUE(content.find("</chain>") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("inner") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception with template placeholders
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithMultiplePlaceholders) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_multi_ph.txt");

    std::runtime_error ex("timeout");
    logger.error(ex, "Failed {op} on {resource} for {user}", "op", "READ", "resource", "/api/data", "user", "alice");

    logger.flush();
    TestUtils::waitForFileContent("ex_multi_ph.txt");
    std::string content = TestUtils::readLogFile("ex_multi_ph.txt");

    EXPECT_TRUE(content.find("timeout") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception with context
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithContext) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_ctx.txt");

    logger.setContext("requestId", "req-123");
    std::runtime_error ex("DB error");
    logger.error(ex, "Query failed");

    logger.flush();
    TestUtils::waitForFileContent("ex_ctx.txt");
    std::string content = TestUtils::readLogFile("ex_ctx.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"requestId\":\"req-123\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception with tags
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithTags) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_tags.txt");

    std::runtime_error ex("auth failed");
    logger.error(ex, "[auth] Login failed for {user}", "bob");

    logger.flush();
    TestUtils::waitForFileContent("ex_tags.txt");
    std::string content = TestUtils::readLogFile("ex_tags.txt");

    EXPECT_TRUE(content.find("\"tags\":[\"auth\"]") != std::string::npos);
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception with scoped context
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithScopedContext) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_scope.txt");

    {
        auto scope = logger.scope({{"txId", "tx-999"}});
        std::runtime_error ex("rollback");
        logger.error(ex, "Transaction failed");
    }

    logger.flush();
    TestUtils::waitForFileContent("ex_scope.txt");
    std::string content = TestUtils::readLogFile("ex_scope.txt");

    EXPECT_TRUE(content.find("\"txId\":\"tx-999\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Regression: log without exception still works identically
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, NormalLogUnchanged) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("ex_normal.txt");

    logger.info("Normal message {val}", "42");

    logger.flush();
    TestUtils::waitForFileContent("ex_normal.txt");
    std::string content = TestUtils::readLogFile("ex_normal.txt");

    EXPECT_TRUE(content.find("Normal message 42") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") == std::string::npos);
    EXPECT_TRUE(content.find("exception") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception with format specifiers in template
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithFormatSpec) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_fmtspec.txt");

    std::runtime_error ex("overflow");
    logger.error(ex, "Value was {val:.2f}", 3.14159);

    logger.flush();
    TestUtils::waitForFileContent("ex_fmtspec.txt");
    std::string content = TestUtils::readLogFile("ex_fmtspec.txt");

    EXPECT_TRUE(content.find("Value was 3.14") != std::string::npos);
    EXPECT_TRUE(content.find("overflow") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception type with special characters in what()
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionMessageWithSpecialChars) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_special.txt");

    std::runtime_error ex("error with \"quotes\" and \\backslash");
    logger.error(ex, "Failure");

    logger.flush();
    TestUtils::waitForFileContent("ex_special.txt");
    std::string content = TestUtils::readLogFile("ex_special.txt");

    EXPECT_TRUE(content.find("\\\"quotes\\\"") != std::string::npos);
    EXPECT_TRUE(content.find("\\\\backslash") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Multiple sinks with exception
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, MultiSinkExceptionOutput) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_multi_hr.txt");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_multi_json.txt");
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("ex_multi_cjson.txt");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("ex_multi_xml.txt");

    std::runtime_error ex("multi-sink error");
    logger.error(ex, "Testing {sink} sinks", "multiple");

    logger.flush();
    TestUtils::waitForFileContent("ex_multi_hr.txt");
    TestUtils::waitForFileContent("ex_multi_json.txt");
    TestUtils::waitForFileContent("ex_multi_cjson.txt");
    TestUtils::waitForFileContent("ex_multi_xml.txt");

    std::string hr = TestUtils::readLogFile("ex_multi_hr.txt");
    std::string json = TestUtils::readLogFile("ex_multi_json.txt");
    std::string cjson = TestUtils::readLogFile("ex_multi_cjson.txt");
    std::string xml = TestUtils::readLogFile("ex_multi_xml.txt");

    EXPECT_TRUE(hr.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(json.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(cjson.find("\"@x\":\"") != std::string::npos);
    EXPECT_TRUE(xml.find("<exception type=\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// std::exception base class
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, BaseStdException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_base.txt");

    class CustomException : public std::exception {
    public:
        const char* what() const noexcept override { return "custom error"; }
    };

    CustomException ex;
    logger.error(ex, "Custom exception caught");

    logger.flush();
    TestUtils::waitForFileContent("ex_base.txt");
    std::string content = TestUtils::readLogFile("ex_base.txt");

    EXPECT_TRUE(content.find("Custom exception caught") != std::string::npos);
    EXPECT_TRUE(content.find("custom error") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception with empty what() message
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithEmptyWhat) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_empty_what.txt");

    class EmptyException : public std::exception {
    public:
        const char* what() const noexcept override { return ""; }
    };

    EmptyException ex;
    logger.error(ex, "Caught empty");

    logger.flush();
    TestUtils::waitForFileContent("ex_empty_what.txt");
    std::string content = TestUtils::readLogFile("ex_empty_what.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"message\":\"\"") != std::string::npos);
}

// ---------------------------------------------------------------------------
// {exception} OutputTemplate token with no exception attached (empty string)
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, OutputTemplateExceptionTokenNoException) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    logger.addSink<minta::FileSink>("ex_tpl_no_ex.txt");
    logger.sink("sink_0").outputTemplate("{message}{exception}");

    logger.info("Normal message");

    logger.flush();
    TestUtils::waitForFileContent("ex_tpl_no_ex.txt");
    std::string content = TestUtils::readLogFile("ex_tpl_no_ex.txt");

    EXPECT_TRUE(content.find("Normal message") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") == std::string::npos);
    EXPECT_TRUE(content.find("exception") == std::string::npos);
}

TEST_F(ExceptionAttachmentTest, OutputTemplateExceptionTokenWithException) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_tpl_with_ex.txt");
    logger.sink("sink_0").outputTemplate("{message} | {exception}");

    std::runtime_error ex("kaboom");
    logger.error(ex, "Something failed");

    logger.flush();
    TestUtils::waitForFileContent("ex_tpl_with_ex.txt");
    std::string content = TestUtils::readLogFile("ex_tpl_with_ex.txt");

    EXPECT_TRUE(content.find("Something failed") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("kaboom") != std::string::npos);
}

// ---------------------------------------------------------------------------
// JSON exception.chain field with nested exceptions
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, JsonFormatterNestedExceptionChain) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_json_chain.txt");

    try {
        try {
            throw std::runtime_error("db connection lost");
        } catch (...) {
            std::throw_with_nested(std::logic_error("query failed"));
        }
    } catch (const std::exception& ex) {
        logger.error(ex, "Request error");
    }

    logger.flush();
    TestUtils::waitForFileContent("ex_json_chain.txt");
    std::string content = TestUtils::readLogFile("ex_json_chain.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"chain\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("db connection lost") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, JsonFormatterNoChainWhenNotNested) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_json_nochain.txt");

    std::runtime_error ex("simple error");
    logger.error(ex, "Failure");

    logger.flush();
    TestUtils::waitForFileContent("ex_json_nochain.txt");
    std::string content = TestUtils::readLogFile("ex_json_nochain.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("\"chain\"") == std::string::npos);
}

// ---------------------------------------------------------------------------
// Null what() fallback behavior
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, NullWhatFallback) {
    class NullWhatException : public std::exception {
    public:
        const char* what() const noexcept override { return nullptr; }
    };

    NullWhatException ex;
    auto info = minta::detail::extractExceptionInfo(ex);
    EXPECT_EQ(info.message, "(no message)");
}

TEST_F(ExceptionAttachmentTest, NullWhatLogOutput) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_null_what.txt");

    class NullWhatException : public std::exception {
    public:
        const char* what() const noexcept override { return nullptr; }
    };

    NullWhatException ex;
    logger.error(ex, "Caught null-what exception");

    logger.flush();
    TestUtils::waitForFileContent("ex_null_what.txt");
    std::string content = TestUtils::readLogFile("ex_null_what.txt");

    EXPECT_TRUE(content.find("Caught null-what exception") != std::string::npos);
    EXPECT_TRUE(content.find("(no message)") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Exception + source location combined usage
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, ExceptionWithSourceLocation) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_srcloc.txt");
    logger.setCaptureSourceLocation(true);

    std::runtime_error ex("auth failed");
    logger.logWithSourceLocationAndException(
        minta::LogLevel::ERROR,
        "test_file.cpp", 42, "testFunc",
        ex, "Login failed for {user}", "bob");

    logger.flush();
    TestUtils::waitForFileContent("ex_srcloc.txt");
    std::string content = TestUtils::readLogFile("ex_srcloc.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("auth failed") != std::string::npos);
    EXPECT_TRUE(content.find("\"file\":\"test_file.cpp\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"line\":42") != std::string::npos);
    EXPECT_TRUE(content.find("\"function\":\"testFunc\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"user\":\"bob\"") != std::string::npos);
}

TEST_F(ExceptionAttachmentTest, ExceptionWithSourceLocationMacro) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink, minta::JsonFormatter>("ex_srcloc_macro.txt");
    logger.setCaptureSourceLocation(true);

    std::runtime_error ex("timeout");
    logger.logWithSourceLocationAndException(
        minta::LogLevel::ERROR,
        LUNAR_LOG_CONTEXT,
        ex, "Operation timed out");

    logger.flush();
    TestUtils::waitForFileContent("ex_srcloc_macro.txt");
    std::string content = TestUtils::readLogFile("ex_srcloc_macro.txt");

    EXPECT_TRUE(content.find("\"exception\":{") != std::string::npos);
    EXPECT_TRUE(content.find("timeout") != std::string::npos);
    EXPECT_TRUE(content.find("\"file\":\"") != std::string::npos);
    EXPECT_TRUE(content.find("\"line\":") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Named constant for depth limit (kMaxNestedExceptionDepth)
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, DepthLimitConstantAccessible) {
    EXPECT_EQ(minta::detail::kMaxNestedExceptionDepth, 20);
}

// ---------------------------------------------------------------------------
// safeWhat utility
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, SafeWhatWithNormalException) {
    std::runtime_error ex("hello");
    EXPECT_STREQ(minta::detail::safeWhat(ex), "hello");
}

TEST_F(ExceptionAttachmentTest, SafeWhatWithEmptyMessage) {
    std::runtime_error ex("");
    EXPECT_STREQ(minta::detail::safeWhat(ex), "");
}

// ---------------------------------------------------------------------------
// Depth limit truncation (kMaxNestedExceptionDepth = 20)
// ---------------------------------------------------------------------------

namespace {
    // Build a chain of N nested exceptions via recursive throw_with_nested.
    // Each level wraps the previous with "level_<depth>".
    void throwNestedChain(int depth, int maxDepth) {
        if (depth >= maxDepth) {
            throw std::runtime_error("level_0");
        }
        try {
            throwNestedChain(depth + 1, maxDepth);
        } catch (...) {
            std::throw_with_nested(
                std::runtime_error("level_" + std::to_string(maxDepth - depth)));
        }
    }
} // anonymous namespace

TEST_F(ExceptionAttachmentTest, DepthLimitTruncatesAt20) {
    const int chainLength = 25;
    try {
        throwNestedChain(0, chainLength);
    } catch (const std::exception& ex) {
        auto info = minta::detail::extractExceptionInfo(ex);

        EXPECT_EQ(info.message, "level_25");

        // Chain unwinding produces 20 entries (depth 0..19):
        //   level_24 (depth 0), level_23 (depth 1), ..., level_5 (depth 19).
        // At depth 20 the guard fires, so level_4..level_0 are absent.
        EXPECT_TRUE(info.chain.find("level_24") != std::string::npos);
        EXPECT_TRUE(info.chain.find("level_5") != std::string::npos);

        EXPECT_TRUE(info.chain.find("level_4") == std::string::npos);
        EXPECT_TRUE(info.chain.find("level_0") == std::string::npos);

        int newlines = 0;
        for (size_t i = 0; i < info.chain.size(); ++i) {
            if (info.chain[i] == '\n') ++newlines;
        }
        EXPECT_EQ(newlines, 19);
    }
}

TEST_F(ExceptionAttachmentTest, DepthLimitExact20PassesThrough) {
    // A chain of exactly 20 nested levels should NOT be truncated.
    const int chainLength = 20;
    try {
        throwNestedChain(0, chainLength);
    } catch (const std::exception& ex) {
        auto info = minta::detail::extractExceptionInfo(ex);
        EXPECT_EQ(info.message, "level_20");

        // All 20 inner levels (level_1 through level_19, plus level_0)
        // should be present.
        EXPECT_TRUE(info.chain.find("level_0") != std::string::npos);
        EXPECT_TRUE(info.chain.find("level_19") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// Null what() through the exception-only log path (no message template)
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, NullWhatExceptionOnlyPath) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_null_what_only.txt");

    class NullWhatException : public std::exception {
    public:
        const char* what() const noexcept override { return nullptr; }
    };

    NullWhatException ex;
    logger.error(ex);

    logger.flush();
    TestUtils::waitForFileContent("ex_null_what_only.txt");
    std::string content = TestUtils::readLogFile("ex_null_what_only.txt");

    EXPECT_TRUE(content.find("(no message)") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Chain prefix consistency between OutputTemplate and HumanReadableFormatter
// ---------------------------------------------------------------------------

TEST_F(ExceptionAttachmentTest, OutputTemplateChainPrefixMatchesDefault) {
    minta::LunarLog logger(minta::LogLevel::ERROR, false);
    logger.addSink<minta::FileSink>("ex_chain_tpl.txt");
    logger.sink("sink_0").outputTemplate("{exception}");

    try {
        try {
            throw std::runtime_error("inner");
        } catch (...) {
            std::throw_with_nested(std::logic_error("outer"));
        }
    } catch (const std::exception& ex) {
        logger.error(ex, "fail");
    }

    logger.flush();
    TestUtils::waitForFileContent("ex_chain_tpl.txt");
    std::string content = TestUtils::readLogFile("ex_chain_tpl.txt");

    // Both paths should use "  --- " prefix for chain lines.
    EXPECT_TRUE(content.find("  --- ") != std::string::npos);
    EXPECT_TRUE(content.find("runtime_error") != std::string::npos);
    EXPECT_TRUE(content.find("inner") != std::string::npos);
}
