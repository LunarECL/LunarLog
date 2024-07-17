#ifndef LUNAR_LOG_COMMON_HPP
#define LUNAR_LOG_COMMON_HPP

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <vector>
#include <memory>

namespace minta {
#if __cplusplus < 201402L
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
#else
    using std::make_unique;
#endif

    inline std::string formatTimestamp(const std::chrono::system_clock::time_point &time) {
        auto nowTime = std::chrono::system_clock::to_time_t(time);
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << nowMs.count();
        return oss.str();
    }
} // namespace minta

#endif // LUNAR_LOG_COMMON_HPP
