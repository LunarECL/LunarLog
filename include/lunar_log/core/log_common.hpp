#ifndef LUNAR_LOG_COMMON_HPP
#define LUNAR_LOG_COMMON_HPP

#include <string>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <sstream>
#include <iomanip>
#include <locale>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <climits>
#include <vector>
#include <unordered_map>
#include <utility>

#include "../transform/pipe_transform.hpp"

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

    /// FNV-1a hash (32-bit) for template grouping.
    inline uint32_t fnv1a(const std::string &s) {
        uint32_t hash = 0x811c9dc5u;
        for (size_t i = 0; i < s.size(); ++i) {
            hash ^= static_cast<uint32_t>(static_cast<unsigned char>(s[i]));
            hash *= 0x01000193u;
        }
        return hash;
    }

    /// Format a uint32_t as an 8-char lowercase hex string.
    inline std::string toHexString(uint32_t value) {
        char buf[9];
        std::snprintf(buf, sizeof(buf), "%08x", value);
        return std::string(buf);
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
        // Defensive: ensure non-negative milliseconds for pre-epoch time points
        std::snprintf(buf + pos, sizeof(buf) - pos, ".%03d", static_cast<int>((nowMs.count() + 1000) % 1000));
        return std::string(buf);
    }
    // ----------------------------------------------------------------
    // Format helpers (shared by LunarLog and detail::reformatMessage)
    // ----------------------------------------------------------------

    /// Safely parse an integer from a string. Returns fallback on failure.
    inline int safeStoi(const std::string &s, int fallback = 0) {
        if (s.empty()) return fallback;
        for (size_t i = 0; i < s.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return fallback;
        }
        try { return std::stoi(s); } catch (...) { return fallback; }
    }

    /// Try to parse a string as a double. Returns true on success and sets out.
    inline bool tryParseDouble(const std::string &s, double &out) {
        if (s.empty()) return false;
        const char* start = s.c_str();
        char* end = nullptr;
        errno = 0;
        double val = std::strtod(start, &end);
        if (errno == ERANGE || end == start || static_cast<size_t>(end - start) != s.size()) {
            errno = 0;
            return false;
        }
        errno = 0;
        if (std::isinf(val) || std::isnan(val)) return false;
        out = val;
        return true;
    }

    inline double clampForLongLong(double val) {
        if (val != val) return 0.0;
        static const double kMinLL = static_cast<double>(LLONG_MIN + 1);
        static const double kMaxLL = std::nextafter(static_cast<double>(LLONG_MAX), 0.0);
        if (val < kMinLL) return kMinLL;
        if (val > kMaxLL) return kMaxLL;
        return val;
    }

    /// Format a double into a string using the given printf format.
    inline std::string snprintfDouble(const char* fmt, double val) {
        int needed = std::snprintf(nullptr, 0, fmt, val);
        if (needed < 0) return std::string();
        if (static_cast<size_t>(needed) < 256) {
            char stackBuf[256];
            std::snprintf(stackBuf, sizeof(stackBuf), fmt, val);
            return std::string(stackBuf);
        }
        std::vector<char> heapBuf(static_cast<size_t>(needed) + 1);
        std::snprintf(heapBuf.data(), heapBuf.size(), fmt, val);
        return std::string(heapBuf.data());
    }

    /// Format a double with precision.
    inline std::string snprintfDoublePrecision(const char* fmt, int precision, double val) {
        int needed = std::snprintf(nullptr, 0, fmt, precision, val);
        if (needed < 0) return std::string();
        if (static_cast<size_t>(needed) < 256) {
            char stackBuf[256];
            std::snprintf(stackBuf, sizeof(stackBuf), fmt, precision, val);
            return std::string(stackBuf);
        }
        std::vector<char> heapBuf(static_cast<size_t>(needed) + 1);
        std::snprintf(heapBuf.data(), heapBuf.size(), fmt, precision, val);
        return std::string(heapBuf.data());
    }

    /// Split "name:spec" into (name, spec). Returns (placeholder, "") if no colon.
    inline std::pair<std::string, std::string> splitPlaceholder(const std::string &placeholder) {
        size_t colonPos = placeholder.rfind(':');
        if (colonPos == std::string::npos) {
            return std::make_pair(placeholder, std::string());
        }
        return std::make_pair(placeholder.substr(0, colonPos), placeholder.substr(colonPos + 1));
    }

    // ----------------------------------------------------------------
    // Culture-specific formatting utilities
    // ----------------------------------------------------------------

    inline std::locale tryCreateLocale(const std::string& name) {
        if (name.empty() || name == "C" || name == "POSIX") {
            return std::locale::classic();
        }
        // Thread-local multi-entry cache: handles the realistic multi-sink
        // case (3-4 locales) without thrashing. Capped at 8 entries to
        // prevent unbounded growth from programmatic locale creation.
        thread_local std::unordered_map<std::string, std::locale> cache;
        auto it = cache.find(name);
        if (it != cache.end()) {
            return it->second;
        }
        std::locale result = std::locale::classic();
        try {
            result = std::locale(name);
        } catch (...) {
            if (name.find('.') == std::string::npos) {
                try {
                    result = std::locale(name + ".UTF-8");
                } catch (...) {}
            }
        }
        if (cache.size() < 8) {
            cache.emplace(name, result);
        }
        return result;
    }

    /// Format a number with locale-specific thousand/decimal separators.
    /// Used by the :n and :N format specs.
    inline std::string formatCultureNumber(const std::string& value, const std::string& localeName) {
        double numVal;
        if (!tryParseDouble(value, numVal)) return value;

        std::locale loc = tryCreateLocale(localeName);
        std::ostringstream oss;
        oss.imbue(loc);

        bool hasSciNotation = (value.find('e') != std::string::npos ||
                               value.find('E') != std::string::npos);
        if (hasSciNotation) {
            oss << std::fixed << std::setprecision(6) << numVal;
        } else {
            size_t dotPos = value.find('.');
            int precision = 0;
            if (dotPos != std::string::npos) {
                precision = static_cast<int>(value.size() - dotPos - 1);
                if (precision > 15) precision = 15;
            }
            oss << std::fixed << std::setprecision(precision) << numVal;
        }
        return oss.str();
    }

    /// Format a unix timestamp as a locale-aware date/time string.
    /// Used by the :d, :D, :t, :T, :f, :F format specs.
    inline std::string formatCultureDateTime(const std::string& value, char spec, const std::string& localeName) {
        double tsVal;
        if (!tryParseDouble(value, tsVal)) return value;

        time_t t = static_cast<time_t>(tsVal);
        std::tm tmBuf;
#if defined(_MSC_VER)
        localtime_s(&tmBuf, &t);
#else
        localtime_r(&t, &tmBuf);
#endif

        const char* fmt = nullptr;
        switch (spec) {
            case 'd': fmt = "%x"; break;                           // short date
            case 'D': fmt = "%A, %B %d, %Y"; break;               // long date
            case 't': fmt = "%H:%M"; break;                        // short time
            case 'T': fmt = "%H:%M:%S"; break;                     // long time
            case 'f': fmt = "%A, %B %d, %Y %H:%M"; break;         // full date + short time
            case 'F': fmt = "%A, %B %d, %Y %H:%M:%S"; break;      // full date + long time
            default: return value;
        }

        std::locale loc = tryCreateLocale(localeName);
        std::ostringstream oss;
        oss.imbue(loc);
        oss << std::put_time(&tmBuf, fmt);
        std::string result = oss.str();
        return result.empty() ? value : result;
    }

    // ----------------------------------------------------------------
    // Complete format spec application with locale support
    // ----------------------------------------------------------------

    /// Apply a format spec to a string value, with optional locale for culture specs.
    /// Handles all existing specs (.Nf, Nf, C, X, E, P, 0N) plus culture specs (n, N, d, D, t, T, f, F).
    inline std::string applyFormat(const std::string &value, const std::string &spec, const std::string &locale = "C") {
        if (spec.empty()) return value;

        double numVal;

        // Culture-specific: locale-aware number (n / N)
        if (spec == "n" || spec == "N") {
            return formatCultureNumber(value, locale);
        }

        // Culture-specific: date/time (d, D, t, T, f, F)
        if (spec.size() == 1) {
            char c = spec[0];
            if (c == 'd' || c == 'D' || c == 't' || c == 'T' || c == 'f' || c == 'F') {
                return formatCultureDateTime(value, c, locale);
            }
        }

        // Fixed-point: .Nf (e.g. ".2f", ".4f")
        if (spec.size() >= 2 && spec[0] == '.' && spec.back() == 'f') {
            if (!tryParseDouble(value, numVal)) return value;
            std::string digits = spec.substr(1, spec.size() - 2);
            int precision = safeStoi(digits, 6);
            if (precision > 50) precision = 50;
            return snprintfDoublePrecision("%.*f", precision, numVal);
        }

        // Fixed-point shorthand: Nf (e.g. "2f", "4f")
        if (spec.size() >= 2 && spec.back() == 'f' && std::isdigit(static_cast<unsigned char>(spec[0]))) {
            if (!tryParseDouble(value, numVal)) return value;
            int precision = safeStoi(spec.substr(0, spec.size() - 1), 6);
            if (precision > 50) precision = 50;
            return snprintfDoublePrecision("%.*f", precision, numVal);
        }

        // Currency: C or c
        if (spec == "C" || spec == "c") {
            if (!tryParseDouble(value, numVal)) return value;
            if (numVal < 0) {
                std::string formatted = snprintfDouble("%.2f", -numVal);
                if (formatted == "0.00") return "$0.00";
                return "-$" + formatted;
            } else {
                std::string formatted = snprintfDouble("%.2f", numVal);
                return "$" + formatted;
            }
        }

        // Hex: X (upper) or x (lower)
        if (spec == "X" || spec == "x") {
            if (!tryParseDouble(value, numVal)) return value;
            char buf[64];
            numVal = clampForLongLong(numVal);
            long long intVal = static_cast<long long>(numVal);
            unsigned long long uval;
            std::string result;
            if (intVal < 0) {
                result = "-";
                uval = 0ULL - static_cast<unsigned long long>(intVal);
            } else {
                uval = static_cast<unsigned long long>(intVal);
            }
            if (spec == "X") {
                std::snprintf(buf, sizeof(buf), "%llX", uval);
            } else {
                std::snprintf(buf, sizeof(buf), "%llx", uval);
            }
            result += buf;
            return result;
        }

        // Scientific: E (upper) or e (lower)
        if (spec == "E" || spec == "e") {
            if (!tryParseDouble(value, numVal)) return value;
            char buf[64];
            if (spec == "E") {
                std::snprintf(buf, sizeof(buf), "%E", numVal);
            } else {
                std::snprintf(buf, sizeof(buf), "%e", numVal);
            }
            return std::string(buf);
        }

        // Percentage: P or p
        if (spec == "P" || spec == "p") {
            if (!tryParseDouble(value, numVal)) return value;
            double pct = numVal * 100.0;
            if (!std::isfinite(pct)) pct = numVal;
            return snprintfDouble("%.2f", pct) + "%";
        }

        // Zero-padded integer: 0N (e.g. "04", "08")
        if (spec.size() >= 2 && spec[0] == '0' && std::isdigit(static_cast<unsigned char>(spec[1]))) {
            if (!tryParseDouble(value, numVal)) return value;
            char buf[64];
            numVal = clampForLongLong(numVal);
            int width = safeStoi(spec.substr(1), 1);
            if (width > 50) width = 50;
            long long intVal = static_cast<long long>(numVal);
            if (intVal < 0) {
                unsigned long long absVal = 0ULL - static_cast<unsigned long long>(intVal);
                std::snprintf(buf, sizeof(buf), "-%0*llu", width, absVal);
            } else {
                std::snprintf(buf, sizeof(buf), "%0*lld", width, intVal);
            }
            return std::string(buf);
        }

        // Unknown spec — return value as-is
        return value;
    }

    // ----------------------------------------------------------------
    // Shared placeholder iterator — single source of truth for
    // operator stripping, validation, and name/spec splitting.
    // ----------------------------------------------------------------

    /// Return true when every character in @p name is an ASCII digit.
    /// Used to distinguish indexed placeholders ({0}, {1}) from named ones ({user}).
    inline bool isIndexedPlaceholder(const std::string &name) {
        if (name.empty()) return false;
        for (size_t i = 0; i < name.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(name[i]))) return false;
        }
        return true;
    }

    struct ParsedPlaceholder {
        size_t startPos;
        size_t endPos;
        std::string name;
        std::string fullContent;
        std::string spec;
        char op;  // '@', '$', or 0
        std::vector<Transform> transforms;
        int indexedArg;  // >= 0 for indexed ({0},{1},...), -1 for named
    };

    template<typename Callback>
    inline void forEachPlaceholder(const std::string &templateStr, Callback callback) {
        for (size_t i = 0; i < templateStr.length(); ++i) {
            if (templateStr[i] == '{') {
                if (i + 1 < templateStr.length() && templateStr[i + 1] == '{') {
                    ++i;
                    continue;
                }
                size_t endPos = templateStr.find('}', i);
                if (endPos == std::string::npos) break;
                std::string content = templateStr.substr(i + 1, endPos - i - 1);
                char op = 0;
                std::string nameContent = content;
                if (!content.empty() && (content[0] == '@' || content[0] == '$')) {
                    op = content[0];
                    nameContent = content.substr(1);
                    if (nameContent.empty() || nameContent[0] == '@' || nameContent[0] == '$'
                        || !(std::isalnum(static_cast<unsigned char>(nameContent[0])) || nameContent[0] == '_')) {
                        i = endPos;
                        continue;
                    }
                }
                std::string nameSpec = nameContent;
                std::vector<Transform> transforms;
                size_t pipePos = nameContent.find('|');
                if (pipePos != std::string::npos) {
                    nameSpec = nameContent.substr(0, pipePos);
                    transforms = parseTransforms(nameContent.substr(pipePos + 1));
                }
                auto parts = splitPlaceholder(nameSpec);
                int idxArg = isIndexedPlaceholder(parts.first) ? safeStoi(parts.first, -1) : -1;
                callback(ParsedPlaceholder{i, endPos, parts.first, content, parts.second, op, std::move(transforms), idxArg});
                i = endPos;
            } else if (templateStr[i] == '}') {
                if (i + 1 < templateStr.length() && templateStr[i + 1] == '}') {
                    ++i;
                }
            }
        }
    }

    // ----------------------------------------------------------------
    // Shared template walker — single source of truth for the
    // template-walk algorithm used by both reformatMessage and
    // LunarLog::formatMessage.  Parameterized on placeholder type
    // (works with ParsedPlaceholder and LunarLog::PlaceholderInfo;
    // both expose startPos, endPos, spec).
    // ----------------------------------------------------------------

    template<typename PlaceholderType>
    inline std::string walkTemplate(const std::string &templateStr,
                                    const std::vector<PlaceholderType> &placeholders,
                                    const std::vector<std::string> &values,
                                    const std::string &locale) {
        std::string result;
        result.reserve(templateStr.length());
        size_t phIdx = 0;
        size_t pos = 0;

        while (pos < templateStr.length()) {
            if (phIdx < placeholders.size() && pos == placeholders[phIdx].startPos) {
                int valueIdx = placeholders[phIdx].indexedArg >= 0
                               ? placeholders[phIdx].indexedArg
                               : static_cast<int>(phIdx);
                if (valueIdx >= 0 && static_cast<size_t>(valueIdx) < values.size()) {
                    std::string formatted = applyFormat(values[valueIdx], placeholders[phIdx].spec, locale);
                    if (!placeholders[phIdx].transforms.empty()) {
                        formatted = applyTransforms(formatted, placeholders[phIdx].transforms);
                    }
                    result += formatted;
                } else if (placeholders[phIdx].indexedArg >= 0) {
                    // Indexed out-of-range renders as empty string
                } else {
                    result.append(templateStr, pos, placeholders[phIdx].endPos - pos + 1);
                }
                pos = placeholders[phIdx].endPos + 1;
                ++phIdx;
            } else if (templateStr[pos] == '{' && pos + 1 < templateStr.length() && templateStr[pos + 1] == '{') {
                result += '{';
                pos += 2;
            } else if (templateStr[pos] == '}' && pos + 1 < templateStr.length() && templateStr[pos + 1] == '}') {
                result += '}';
                pos += 2;
            } else {
                size_t litStart = pos;
                ++pos;
                while (pos < templateStr.length()) {
                    if (phIdx < placeholders.size() && pos == placeholders[phIdx].startPos) break;
                    if (templateStr[pos] == '{' && pos + 1 < templateStr.length() && templateStr[pos + 1] == '{') break;
                    if (templateStr[pos] == '}' && pos + 1 < templateStr.length() && templateStr[pos + 1] == '}') break;
                    ++pos;
                }
                result.append(templateStr, litStart, pos - litStart);
            }
        }
        return result;
    }

    // ----------------------------------------------------------------
    // Message re-rendering with a different locale
    // ----------------------------------------------------------------

    inline std::string reformatMessage(const std::string &templateStr,
                                       const std::vector<std::string> &values,
                                       const std::string &locale) {
        std::vector<ParsedPlaceholder> spans;
        forEachPlaceholder(templateStr, [&](const ParsedPlaceholder& ph) {
            spans.push_back(ph);
        });
        return walkTemplate(templateStr, spans, values, locale);
    }

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_COMMON_HPP
