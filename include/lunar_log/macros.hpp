#ifndef LUNAR_LOG_MACROS_HPP
#define LUNAR_LOG_MACROS_HPP

#ifndef LUNAR_LOG_NO_MACROS

// Generic macro --- level check avoids argument evaluation when disabled
#define LUNAR_LOG(logger, level, ...) \
    do { \
        auto& lunar_log_ref_ = (logger); \
        auto  lunar_log_lvl_ = (level); \
        if (lunar_log_lvl_ >= lunar_log_ref_.getMinLevel()) { \
            lunar_log_ref_.logWithSourceLocation( \
                lunar_log_lvl_, __FILE__, __LINE__, \
                __func__, \
                __VA_ARGS__); \
        } \
    } while (0)

#define LUNAR_TRACE(logger, ...) LUNAR_LOG((logger), ::minta::LogLevel::TRACE, __VA_ARGS__)
#define LUNAR_DEBUG(logger, ...) LUNAR_LOG((logger), ::minta::LogLevel::DEBUG, __VA_ARGS__)
#define LUNAR_INFO(logger, ...)  LUNAR_LOG((logger), ::minta::LogLevel::INFO,  __VA_ARGS__)
#define LUNAR_WARN(logger, ...)  LUNAR_LOG((logger), ::minta::LogLevel::WARN,  __VA_ARGS__)
#define LUNAR_ERROR(logger, ...) LUNAR_LOG((logger), ::minta::LogLevel::ERROR, __VA_ARGS__)
#define LUNAR_FATAL(logger, ...) LUNAR_LOG((logger), ::minta::LogLevel::FATAL, __VA_ARGS__)

// Exception variants
#define LUNAR_LOG_EX(logger, level, ex, ...) \
    do { \
        auto& lunar_log_ref_ = (logger); \
        auto  lunar_log_lvl_ = (level); \
        if (lunar_log_lvl_ >= lunar_log_ref_.getMinLevel()) { \
            lunar_log_ref_.logWithSourceLocationAndException( \
                lunar_log_lvl_, __FILE__, __LINE__, \
                __func__, \
                (ex), __VA_ARGS__); \
        } \
    } while (0)

#define LUNAR_TRACE_EX(logger, ex, ...) LUNAR_LOG_EX((logger), ::minta::LogLevel::TRACE, (ex), __VA_ARGS__)
#define LUNAR_DEBUG_EX(logger, ex, ...) LUNAR_LOG_EX((logger), ::minta::LogLevel::DEBUG, (ex), __VA_ARGS__)
#define LUNAR_INFO_EX(logger, ex, ...)  LUNAR_LOG_EX((logger), ::minta::LogLevel::INFO,  (ex), __VA_ARGS__)
#define LUNAR_WARN_EX(logger, ex, ...)  LUNAR_LOG_EX((logger), ::minta::LogLevel::WARN,  (ex), __VA_ARGS__)
#define LUNAR_ERROR_EX(logger, ex, ...) LUNAR_LOG_EX((logger), ::minta::LogLevel::ERROR, (ex), __VA_ARGS__)
#define LUNAR_FATAL_EX(logger, ex, ...) LUNAR_LOG_EX((logger), ::minta::LogLevel::FATAL, (ex), __VA_ARGS__)

#endif // LUNAR_LOG_NO_MACROS

#endif // LUNAR_LOG_MACROS_HPP
