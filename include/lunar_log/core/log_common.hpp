#ifndef LUNAR_LOG_COMMON_HPP
#define LUNAR_LOG_COMMON_HPP

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
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

        std::ostringstream oss;
        oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
        return oss.str();
    }
} // namespace detail

    using detail::formatTimestamp;
} // namespace minta

#endif // LUNAR_LOG_COMMON_HPP
