#ifndef LUNAR_LOG_COMMON_HPP
#define LUNAR_LOG_COMMON_HPP

#include <string>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <memory>

namespace minta {
namespace detail {
    template<typename T, typename... Args>
    inline std::unique_ptr<T> make_unique(Args&&... args) {
#if (__cplusplus >= 201402L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)
        return std::make_unique<T>(std::forward<Args>(args)...);
#else
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
#endif
    }

    inline std::string formatTimestamp(const std::chrono::system_clock::time_point &time) {
        auto nowTime = std::chrono::system_clock::to_time_t(time);
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

        std::tm tmBuf;
#if defined(_MSC_VER)
        localtime_s(&tmBuf, &nowTime);
#else
        localtime_r(&nowTime, &tmBuf);
#endif

        char buf[32];
        size_t pos = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
        if (pos == 0) {
            buf[0] = '\0';
        }
        std::snprintf(buf + pos, sizeof(buf) - pos, ".%03d", static_cast<int>((nowMs.count() + 1000) % 1000));
        return std::string(buf);
    }
} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_COMMON_HPP
