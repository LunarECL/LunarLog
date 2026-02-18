// Negative test: verify that defining LUNAR_LOG_NO_MACROS before including the
// header suppresses ALL logging macros.  This MUST be a separate translation
// unit so the #define takes effect before the first include of the header.

#define LUNAR_LOG_NO_MACROS
#include <gtest/gtest.h>
#include "lunar_log.hpp"

// ---------------------------------------------------------------------------
// The core library should still be usable — only convenience macros are gone.
// ---------------------------------------------------------------------------

TEST(NoMacrosTest, MacrosNotDefinedWhenOptedOut) {
#ifdef LUNAR_LOG
    FAIL() << "LUNAR_LOG should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_TRACE
    FAIL() << "LUNAR_TRACE should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_DEBUG
    FAIL() << "LUNAR_DEBUG should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_INFO
    FAIL() << "LUNAR_INFO should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_WARN
    FAIL() << "LUNAR_WARN should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_ERROR
    FAIL() << "LUNAR_ERROR should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_FATAL
    FAIL() << "LUNAR_FATAL should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_LOG_EX
    FAIL() << "LUNAR_LOG_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_TRACE_EX
    FAIL() << "LUNAR_TRACE_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_DEBUG_EX
    FAIL() << "LUNAR_DEBUG_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_INFO_EX
    FAIL() << "LUNAR_INFO_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_WARN_EX
    FAIL() << "LUNAR_WARN_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_ERROR_EX
    FAIL() << "LUNAR_ERROR_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif

#ifdef LUNAR_FATAL_EX
    FAIL() << "LUNAR_FATAL_EX should NOT be defined when LUNAR_LOG_NO_MACROS is set";
#else
    SUCCEED();
#endif
}

// ---------------------------------------------------------------------------
// Verify the logger itself still works via direct API calls.
// ---------------------------------------------------------------------------

TEST(NoMacrosTest, LoggerStillWorksWithoutMacros) {
    minta::LunarLog logger(minta::LogLevel::INFO, false);
    // Must compile and not crash — macros are gone but the API is intact
    logger.info("Direct API call works");
    SUCCEED();
}
