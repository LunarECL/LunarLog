#ifndef LUNAR_LOG_SHARED_MUTEX_HPP
#define LUNAR_LOG_SHARED_MUTEX_HPP

#include <mutex>
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#include <shared_mutex>
#endif

namespace minta {
namespace detail {

// C++17: use shared_mutex for read-heavy hot paths (template cache,
// locale, context, global filters). Falls back to std::mutex on C++11/14.
#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
    using SharedMutex = std::shared_mutex;
    template<typename M> using ReadLock  = std::shared_lock<M>;
#else
    using SharedMutex = std::mutex;
    template<typename M> using ReadLock  = std::lock_guard<M>;
#endif
    // WriteLock is always unique_lock for API parity across standards
    template<typename M> using WriteLock = std::unique_lock<M>;

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_SHARED_MUTEX_HPP
