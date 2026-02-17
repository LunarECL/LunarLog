#ifndef LUNAR_LOG_ROLLING_POLICY_HPP
#define LUNAR_LOG_ROLLING_POLICY_HPP

#include <string>
#include <cstdint>

namespace minta {

    enum class RollInterval {
        None,
        Daily,
        Hourly
    };

    class RollingPolicy {
    public:
        /// Size-based rolling: rotate when the current file reaches maxBytes.
        static RollingPolicy size(const std::string& path, std::uint64_t maxBytes) {
            RollingPolicy p;
            p.m_basePath = path;
            p.m_maxSizeBytes = maxBytes;
            p.m_rollInterval = RollInterval::None;
            return p;
        }

        /// Daily time-based rolling.
        static RollingPolicy daily(const std::string& path) {
            RollingPolicy p;
            p.m_basePath = path;
            p.m_maxSizeBytes = 0;
            p.m_rollInterval = RollInterval::Daily;
            return p;
        }

        /// Hourly time-based rolling.
        static RollingPolicy hourly(const std::string& path) {
            RollingPolicy p;
            p.m_basePath = path;
            p.m_maxSizeBytes = 0;
            p.m_rollInterval = RollInterval::Hourly;
            return p;
        }

        /// Maximum number of rolled files to keep (0 = unlimited).
        RollingPolicy& maxFiles(unsigned int n) {
            m_maxFiles = n;
            return *this;
        }

        /// Maximum size per file (enables hybrid size+time rolling).
        RollingPolicy& maxSize(std::uint64_t bytes) {
            m_maxSizeBytes = bytes;
            return *this;
        }

        /// Maximum total size of all rolled files combined (0 = unlimited).
        /// When exceeded, the oldest rolled files are deleted until under the limit.
        RollingPolicy& maxTotalSize(std::uint64_t bytes) {
            m_maxTotalSize = bytes;
            return *this;
        }

        // --- Accessors ---
        const std::string& basePath()         const { return m_basePath; }
        std::uint64_t      maxSizeBytes()     const { return m_maxSizeBytes; }
        RollInterval       rollInterval()     const { return m_rollInterval; }
        unsigned int       maxFilesCount()    const { return m_maxFiles; }
        std::uint64_t      maxTotalSizeBytes() const { return m_maxTotalSize; }

    private:
        RollingPolicy() : m_maxSizeBytes(0), m_rollInterval(RollInterval::None), m_maxFiles(0), m_maxTotalSize(0) {}

        std::string    m_basePath;
        std::uint64_t  m_maxSizeBytes;
        RollInterval   m_rollInterval;
        unsigned int   m_maxFiles;
        std::uint64_t  m_maxTotalSize;
    };

} // namespace minta

#endif // LUNAR_LOG_ROLLING_POLICY_HPP
