#ifndef LUNAR_LOG_ROLLING_FILE_SINK_HPP
#define LUNAR_LOG_ROLLING_FILE_SINK_HPP

#include "sink_interface.hpp"
#include "../core/rolling_policy.hpp"
#include "../formatter/human_readable_formatter.hpp"
#include <fstream>
#include <mutex>
#include <string>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <climits>
#include <deque>
#include <vector>

#include <sys/stat.h>
#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

namespace minta {

    class RollingFileSink : public ISink {
    public:
        explicit RollingFileSink(const RollingPolicy& policy)
            : m_policy(policy)
            , m_currentSize(0)
            , m_lastPeriod()
            , m_fileOpen(false)
            , m_sizeRollIndex(0)
            , m_lastPeriodCheckTime(0)
        {
            setFormatter(detail::make_unique<HumanReadableFormatter>());
            splitBasePath();
        }

        ~RollingFileSink() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_fileOpen) {
                m_file.close();
                m_fileOpen = false;
            }
        }

        /// Set the formatter type. Must be called during initialization only,
        /// before any calls to write(). Not thread-safe with concurrent logging.
        template<typename FormatterType>
        void useFormatter() {
            setFormatter(detail::make_unique<FormatterType>());
        }

        void write(const LogEntry& entry) override {
            IFormatter* fmt = formatter();
            if (!fmt) return;
            std::string formatted = fmt->format(entry);

            std::lock_guard<std::mutex> lock(m_mutex);
            ensureOpen();
            if (needsRotation()) {
                rotate();
            }
            writeToFile(formatted);
        }

    private:
        void splitBasePath() {
            const std::string& path = m_policy.basePath();
            size_t dotPos = path.rfind('.');
            size_t slashPos = path.find_last_of("/\\");
            if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
                m_stem = path.substr(0, dotPos);
                m_ext = path.substr(dotPos);
            } else {
                m_stem = path;
                m_ext.clear();
            }
        }

        static bool mkdirRecursive(const std::string& path) {
            if (path.empty()) return true;
            struct stat st;
            if (stat(path.c_str(), &st) == 0) return true;

            size_t slashPos = path.find_last_of("/\\");
            if (slashPos != std::string::npos && slashPos > 0) {
                if (!mkdirRecursive(path.substr(0, slashPos))) return false;
            }
#ifdef _MSC_VER
            return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
            return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
        }

        void ensureDirectoryExists() {
            const std::string& path = m_policy.basePath();
            size_t slashPos = path.find_last_of("/\\");
            if (slashPos != std::string::npos && slashPos > 0) {
                mkdirRecursive(path.substr(0, slashPos));
            }
        }

        void ensureOpen() {
            if (m_fileOpen) return;
            ensureDirectoryExists();
            m_file.open(m_policy.basePath(), std::ios::app | std::ios::binary);
            if (!m_file.is_open()) {
                std::fprintf(stderr, "RollingFileSink: failed to open file: %s\n",
                             m_policy.basePath().c_str());
                return;
            }
            m_fileOpen = true;
            m_currentSize = getFileSize(m_policy.basePath());
            if (m_policy.rollInterval() != RollInterval::None) {
                m_lastPeriodCheckTime = std::time(nullptr);
                m_lastPeriod = currentPeriodString(m_lastPeriodCheckTime);
            }
            discoverExistingRolledFiles();
        }

        void writeToFile(const std::string& formatted) {
            if (!m_fileOpen) return;
            m_file << formatted << '\n' << std::flush;
            m_currentSize += formatted.size() + 1;
        }

        bool needsRotation() {
            if (!m_fileOpen) return false;
            if (m_policy.maxSizeBytes() > 0 && m_currentSize >= m_policy.maxSizeBytes()) {
                return true;
            }
            if (m_policy.rollInterval() != RollInterval::None) {
                std::time_t now = std::time(nullptr);
                if (now != m_lastPeriodCheckTime) {
                    m_lastPeriodCheckTime = now;
                    std::string current = currentPeriodString(now);
                    if (current != m_lastPeriod) return true;
                }
            }
            return false;
        }

        void rotate() {
            m_file.close();
            m_fileOpen = false;

            std::string rolledName = buildRolledName();
            if (std::rename(m_policy.basePath().c_str(), rolledName.c_str()) == 0) {
                m_rolledFiles.push_back(rolledName);
            } else {
                std::fprintf(stderr, "RollingFileSink: failed to rename %s to %s\n",
                             m_policy.basePath().c_str(), rolledName.c_str());
            }

            if (m_policy.rollInterval() != RollInterval::None) {
                m_lastPeriodCheckTime = std::time(nullptr);
                m_lastPeriod = currentPeriodString(m_lastPeriodCheckTime);
            }

            cleanup();

            m_file.open(m_policy.basePath(), std::ios::app | std::ios::binary);
            if (m_file.is_open()) {
                m_fileOpen = true;
                m_currentSize = getFileSize(m_policy.basePath());
            }
        }

        std::string buildRolledName() {
            bool hasSizePolicy = m_policy.maxSizeBytes() > 0;
            bool hasTimePolicy = m_policy.rollInterval() != RollInterval::None;

            if (hasTimePolicy && hasSizePolicy) {
                std::string period = m_lastPeriod;
                if (period != m_lastRolledPeriod) {
                    m_sizeRollIndex = 0;
                    m_lastRolledPeriod = period;
                }
                m_sizeRollIndex++;
                char idx[16];
                std::snprintf(idx, sizeof(idx), "%03u", m_sizeRollIndex);
                return m_stem + "." + period + "." + idx + m_ext;
            }
            if (hasTimePolicy) {
                // Time-only mode: one file per period. If rotation triggers twice
                // in the same period, the second rename overwrites the first — by
                // design, since time-only means at most one rolled file per period.
                return m_stem + "." + m_lastPeriod + m_ext;
            }
            m_sizeRollIndex++;
            char idx[16];
            std::snprintf(idx, sizeof(idx), "%03u", m_sizeRollIndex);
            return m_stem + "." + idx + m_ext;
        }

        std::string currentPeriodString(std::time_t now) const {
            std::tm tmBuf;
#ifdef _MSC_VER
            localtime_s(&tmBuf, &now);
#else
            localtime_r(&now, &tmBuf);
#endif
            char buf[32];
            if (m_policy.rollInterval() == RollInterval::Daily) {
                std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmBuf);
            } else {
                std::strftime(buf, sizeof(buf), "%Y-%m-%d.%H", &tmBuf);
            }
            return std::string(buf);
        }

        static std::uint64_t getFileSize(const std::string& path) {
            struct stat st;
            if (stat(path.c_str(), &st) != 0) return 0;
            return static_cast<std::uint64_t>(st.st_size);
        }

        static std::time_t getFileMTime(const std::string& path) {
            struct stat st;
            if (stat(path.c_str(), &st) != 0) return 0;
            return st.st_mtime;
        }

        static std::vector<std::string> listDirectory(const std::string& dirPath) {
            std::vector<std::string> entries;
#ifdef _MSC_VER
            struct _finddata_t fileinfo;
            std::string pattern = dirPath + "/*";
            intptr_t handle = _findfirst(pattern.c_str(), &fileinfo);
            if (handle == -1) return entries;
            do {
                std::string name = fileinfo.name;
                if (name != "." && name != "..") {
                    entries.push_back(name);
                }
            } while (_findnext(handle, &fileinfo) == 0);
            _findclose(handle);
#else
            DIR* dir = opendir(dirPath.c_str());
            if (!dir) return entries;
            struct dirent* ent;
            while ((ent = readdir(dir)) != nullptr) {
                std::string name = ent->d_name;
                if (name != "." && name != "..") {
                    entries.push_back(name);
                }
            }
            closedir(dir);
#endif
            return entries;
        }

        static bool allDigits(const char* s, size_t n) {
            if (n == 0) return false;
            for (size_t i = 0; i < n; ++i) {
                if (s[i] < '0' || s[i] > '9') return false;
            }
            return true;
        }

        /// Check if string matches YYYY-MM-DD pattern.
        /// @pre strlen(s) >= 10
        static bool isDatePattern(const char* s) {
            return s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9' &&
                   s[2] >= '0' && s[2] <= '9' && s[3] >= '0' && s[3] <= '9' &&
                   s[4] == '-' &&
                   s[5] >= '0' && s[5] <= '9' && s[6] >= '0' && s[6] <= '9' &&
                   s[7] == '-' &&
                   s[8] >= '0' && s[8] <= '9' && s[9] >= '0' && s[9] <= '9';
        }

        static bool isValidRolledMiddle(const std::string& mid) {
            if (mid.empty()) return false;
            const char* s = mid.c_str();
            size_t len = mid.size();

            // Pure digits (size index: "001", "002", etc.)
            if (allDigits(s, len)) return true;

            // Must start with date YYYY-MM-DD (10 chars)
            if (len < 10 || !isDatePattern(s)) return false;
            if (len == 10) return true;

            // Dot must follow the date
            if (s[10] != '.') return false;
            const char* rest = s + 11;
            size_t restLen = len - 11;
            if (restLen == 0) return false;

            // YYYY-MM-DD.digits (daily+size or hourly time-only)
            if (allDigits(rest, restLen)) return true;

            // YYYY-MM-DD.HH.NNN (hourly+size)
            if (restLen > 3 && rest[0] >= '0' && rest[0] <= '9' &&
                rest[1] >= '0' && rest[1] <= '9' && rest[2] == '.') {
                return allDigits(rest + 3, restLen - 3);
            }
            return false;
        }

        static unsigned int parseDigits(const char* s) {
            unsigned int result = 0;
            while (*s >= '0' && *s <= '9') {
                unsigned int digit = static_cast<unsigned int>(*s - '0');
                if (result > (UINT_MAX - digit) / 10) return UINT_MAX;
                result = result * 10 + digit;
                ++s;
            }
            return result;
        }

        void discoverExistingRolledFiles() {
            m_rolledFiles.clear();
            m_sizeRollIndex = 0;

            // Determine directory and stem filename
            std::string dir;
            std::string stemFilename;
            {
                size_t slashPos = m_stem.find_last_of("/\\");
                if (slashPos != std::string::npos) {
                    dir = m_stem.substr(0, slashPos);
                    stemFilename = m_stem.substr(slashPos + 1);
                } else {
                    dir = ".";
                    stemFilename = m_stem;
                }
            }

            std::string prefix = stemFilename + ".";
            std::vector<std::string> entries = listDirectory(dir);

            // Collect matching rolled files with mtime for sorting
            struct RolledEntry {
                std::string path;
                std::string middle;
                std::time_t mtime;
            };
            std::vector<RolledEntry> found;

            for (size_t i = 0; i < entries.size(); ++i) {
                const std::string& name = entries[i];

                // Must start with stem prefix (e.g. "roll_size.")
                if (name.size() <= prefix.size() ||
                    name.compare(0, prefix.size(), prefix) != 0) {
                    continue;
                }

                // Must end with extension (if any) and extract middle part
                std::string middle;
                if (!m_ext.empty()) {
                    if (name.size() <= prefix.size() + m_ext.size()) continue;
                    if (name.compare(name.size() - m_ext.size(),
                                     m_ext.size(), m_ext) != 0) continue;
                    middle = name.substr(prefix.size(),
                                         name.size() - prefix.size() - m_ext.size());
                } else {
                    middle = name.substr(prefix.size());
                }

                if (middle.empty()) continue;
                if (!isValidRolledMiddle(middle)) continue;

                // Reconstruct full path matching buildRolledName format
                std::string fullPath = m_stem + "." + middle + m_ext;

                RolledEntry re;
                re.path = fullPath;
                re.middle = middle;
                re.mtime = getFileMTime(fullPath);
                found.push_back(re);
            }

            // Sort by mtime (oldest first) for correct cleanup ordering
            std::sort(found.begin(), found.end(),
                      [](const RolledEntry& a, const RolledEntry& b) {
                          return a.mtime < b.mtime;
                      });

            // Populate m_rolledFiles and recover m_sizeRollIndex
            bool hasTimePolicy = m_policy.rollInterval() != RollInterval::None;
            bool hasSizePolicy = m_policy.maxSizeBytes() > 0;

            for (size_t i = 0; i < found.size(); ++i) {
                m_rolledFiles.push_back(found[i].path);

                if (hasSizePolicy) {
                    unsigned int idx = 0;
                    const std::string& mid = found[i].middle;

                    if (!hasTimePolicy) {
                        // Size only: entire middle is the index
                        idx = parseDigits(mid.c_str());
                    } else if (!m_lastPeriod.empty() &&
                               mid.size() > m_lastPeriod.size() + 1 &&
                               mid.compare(0, m_lastPeriod.size(), m_lastPeriod) == 0 &&
                               mid[m_lastPeriod.size()] == '.') {
                        // Hybrid: period.NNN — extract NNN for current period
                        idx = parseDigits(mid.c_str() + m_lastPeriod.size() + 1);
                    }

                    if (idx > m_sizeRollIndex) {
                        m_sizeRollIndex = idx;
                    }
                }
            }
        }

        void cleanup() {
            bool hasMaxFiles = m_policy.maxFilesCount() > 0;
            bool hasMaxTotalSize = m_policy.maxTotalSizeBytes() > 0;
            if (!hasMaxFiles && !hasMaxTotalSize) return;

            if (hasMaxFiles) {
                while (m_rolledFiles.size() > m_policy.maxFilesCount()) {
                    std::remove(m_rolledFiles.front().c_str());
                    m_rolledFiles.pop_front();
                }
            }

            if (hasMaxTotalSize) {
                std::vector<std::uint64_t> sizes;
                std::uint64_t total = 0;
                for (size_t i = 0; i < m_rolledFiles.size(); ++i) {
                    std::uint64_t sz = getFileSize(m_rolledFiles[i]);
                    sizes.push_back(sz);
                    total += sz;
                }
                // sizes[removeIdx] corresponds to the current front of m_rolledFiles
                // because we increment removeIdx in lockstep with pop_front
                size_t removeIdx = 0;
                while (removeIdx < m_rolledFiles.size() && total > m_policy.maxTotalSizeBytes()) {
                    total -= sizes[removeIdx];
                    std::remove(m_rolledFiles.front().c_str());
                    m_rolledFiles.pop_front();
                    removeIdx++;
                }
            }
        }

        RollingPolicy m_policy;
        std::string m_stem;
        std::string m_ext;
        std::ofstream m_file;
        std::mutex m_mutex;
        std::uint64_t m_currentSize;
        std::string m_lastPeriod;
        std::string m_lastRolledPeriod;
        bool m_fileOpen;
        unsigned int m_sizeRollIndex;
        std::time_t m_lastPeriodCheckTime;
        std::deque<std::string> m_rolledFiles;
    };

} // namespace minta

#endif // LUNAR_LOG_ROLLING_FILE_SINK_HPP
