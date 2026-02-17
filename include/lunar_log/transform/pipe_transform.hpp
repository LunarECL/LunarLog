#ifndef LUNAR_LOG_PIPE_TRANSFORM_HPP
#define LUNAR_LOG_PIPE_TRANSFORM_HPP

#include <string>
#include <vector>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <climits>

namespace minta {
namespace detail {

    // ----------------------------------------------------------------
    // Pipe Transform — parse and apply "|transform" chains
    // ----------------------------------------------------------------

    struct Transform {
        std::string name;
        std::string arg;
    };

    // --- Private helpers (number parsing, UTF-8) ---

    /// Parse a double from string. Separate from detail::tryParseDouble
    /// to keep pipe_transform.hpp self-contained (no log_common.hpp dep).
    inline bool pipeParseDouble(const std::string &s, double &out) {
        if (s.empty()) return false;
        const char *start = s.c_str();
        char *end = nullptr;
        errno = 0;
        double val = std::strtod(start, &end);
        if (errno == ERANGE || end == start ||
            static_cast<size_t>(end - start) != s.size()) {
            errno = 0;
            return false;
        }
        errno = 0;
        if (std::isinf(val) || std::isnan(val)) return false;
        out = val;
        return true;
    }

    /// Safe int parser for transform arguments.
    inline int pipeSafeStoi(const std::string &s, int fallback = 0) {
        if (s.empty()) return fallback;
        for (size_t i = 0; i < s.size(); ++i) {
            if (i == 0 && s[i] == '-') continue;
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) return fallback;
        }
        try { return std::stoi(s); } catch (...) { return fallback; }
    }

    /// Count UTF-8 codepoints in a string.
    inline size_t utf8CharCount(const std::string &s) {
        size_t count = 0;
        for (size_t i = 0; i < s.size(); ) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x80)        i += 1;
            else if ((c & 0xE0) == 0xC0) i += 2;
            else if ((c & 0xF0) == 0xE0) i += 3;
            else if ((c & 0xF8) == 0xF0) i += 4;
            else                 i += 1; // invalid byte, skip
            ++count;
        }
        return count;
    }

    /// Truncate a string to maxChars UTF-8 codepoints.
    inline std::string utf8Truncate(const std::string &s, size_t maxChars) {
        size_t count = 0;
        size_t bytePos = 0;
        while (bytePos < s.size() && count < maxChars) {
            unsigned char c = static_cast<unsigned char>(s[bytePos]);
            size_t charLen = 1;
            if (c < 0x80)        charLen = 1;
            else if ((c & 0xE0) == 0xC0) charLen = 2;
            else if ((c & 0xF0) == 0xE0) charLen = 3;
            else if ((c & 0xF8) == 0xF0) charLen = 4;
            // Clamp to string boundary to avoid reading past end
            if (bytePos + charLen > s.size()) break;
            bytePos += charLen;
            ++count;
        }
        return s.substr(0, bytePos);
    }

    /// Serialise a Transform back to its pipe-syntax string form.
    inline std::string transformToString(const Transform &t) {
        if (t.arg.empty()) return t.name;
        return t.name + ":" + t.arg;
    }

    /// Clamp a double to the representable long long range (prevents UB on cast).
    inline double pipeClampLL(double val) {
        if (val != val) return 0.0; // NaN
        static const double kMinLL = static_cast<double>(LLONG_MIN + 1);
        static const double kMaxLL = std::nextafter(static_cast<double>(LLONG_MAX), 0.0);
        if (val < kMinLL) return kMinLL;
        if (val > kMaxLL) return kMaxLL;
        return val;
    }

    // ----------------------------------------------------------------
    // Transform parsing
    // ----------------------------------------------------------------

    /// Parse "comma|truncate:10|quote" into [{comma,""}, {truncate,"10"}, {quote,""}]
    inline std::vector<Transform> parseTransforms(const std::string &pipeStr) {
        std::vector<Transform> result;
        if (pipeStr.empty()) return result;

        size_t start = 0;
        while (start <= pipeStr.size()) {
            size_t end = pipeStr.find('|', start);
            if (end == std::string::npos) end = pipeStr.size();
            if (end > start) {
                std::string token = pipeStr.substr(start, end - start);
                size_t colonPos = token.find(':');
                if (colonPos != std::string::npos) {
                    result.push_back({token.substr(0, colonPos),
                                      token.substr(colonPos + 1)});
                } else {
                    result.push_back({token, std::string()});
                }
            }
            start = end + 1;
        }
        return result;
    }

    // ----------------------------------------------------------------
    // Built-in String Transforms
    // ----------------------------------------------------------------

    inline std::string transformUpper(const std::string &value) {
        std::string result = value;
        for (size_t i = 0; i < result.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(result[i]);
            if (c >= 'a' && c <= 'z') result[i] = static_cast<char>(c - 32);
        }
        return result;
    }

    inline std::string transformLower(const std::string &value) {
        std::string result = value;
        for (size_t i = 0; i < result.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(result[i]);
            if (c >= 'A' && c <= 'Z') result[i] = static_cast<char>(c + 32);
        }
        return result;
    }

    inline std::string transformTrim(const std::string &value) {
        if (value.empty()) return value;
        size_t start = 0;
        while (start < value.size() &&
               std::isspace(static_cast<unsigned char>(value[start]))) ++start;
        if (start == value.size()) return std::string();
        size_t end = value.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
        return value.substr(start, end - start);
    }

    /// Limit to N UTF-8 codepoints; append ellipsis if truncated.
    inline std::string transformTruncate(const std::string &value,
                                         const std::string &arg) {
        int n = pipeSafeStoi(arg, -1);
        if (n < 0) return value; // invalid / missing arg
        size_t maxChars = static_cast<size_t>(n);
        size_t charCount = utf8CharCount(value);
        if (charCount <= maxChars) return value;
        // U+2026 HORIZONTAL ELLIPSIS (3 bytes in UTF-8)
        return utf8Truncate(value, maxChars) + "\xe2\x80\xa6";
    }

    /// Right-pad with spaces to N UTF-8 codepoints.
    inline std::string transformPad(const std::string &value,
                                    const std::string &arg) {
        int n = pipeSafeStoi(arg, 0);
        if (n <= 0) return value;
        size_t charCount = utf8CharCount(value);
        if (charCount >= static_cast<size_t>(n)) return value;
        return value + std::string(static_cast<size_t>(n) - charCount, ' ');
    }

    /// Left-pad with spaces to N UTF-8 codepoints.
    inline std::string transformPadLeft(const std::string &value,
                                        const std::string &arg) {
        int n = pipeSafeStoi(arg, 0);
        if (n <= 0) return value;
        size_t charCount = utf8CharCount(value);
        if (charCount >= static_cast<size_t>(n)) return value;
        return std::string(static_cast<size_t>(n) - charCount, ' ') + value;
    }

    /// Wrap in double quotes.
    inline std::string transformQuote(const std::string &value) {
        return "\"" + value + "\"";
    }

    // ----------------------------------------------------------------
    // Built-in Number Transforms
    // ----------------------------------------------------------------

    /// Thousands separator: 1234567 -> "1,234,567", 1234567.89 -> "1,234,567.89"
    inline std::string transformComma(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;

        // Normalize scientific notation to fixed-point
        std::string work = value;
        if (value.find('e') != std::string::npos ||
            value.find('E') != std::string::npos) {
            char buf[64];
            if (numVal == std::floor(numVal) && std::fabs(numVal) < 1e15) {
                std::snprintf(buf, sizeof(buf), "%.0f", numVal);
            } else {
                std::snprintf(buf, sizeof(buf), "%.15g", numVal);
            }
            work = std::string(buf);
        }

        // Split on decimal point
        size_t dotPos = work.find('.');
        std::string intPart = (dotPos != std::string::npos)
                                  ? work.substr(0, dotPos)
                                  : work;
        std::string decPart = (dotPos != std::string::npos)
                                  ? work.substr(dotPos)
                                  : std::string();

        // Handle sign
        std::string prefix;
        if (!intPart.empty() && (intPart[0] == '-' || intPart[0] == '+')) {
            prefix = intPart.substr(0, 1);
            intPart = intPart.substr(1);
        }

        // Add commas every 3 digits from the right
        std::string result;
        int len = static_cast<int>(intPart.size());
        for (int i = 0; i < len; ++i) {
            if (i > 0 && (len - i) % 3 == 0) {
                result += ',';
            }
            result += intPart[i];
        }

        return prefix + result + decPart;
    }

    /// Hex with 0x prefix: 255 -> "0xff"
    inline std::string transformHex(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;
        long long intVal = static_cast<long long>(pipeClampLL(numVal));
        char buf[64];
        if (intVal < 0) {
            unsigned long long uval = 0ULL - static_cast<unsigned long long>(intVal);
            std::snprintf(buf, sizeof(buf), "-0x%llx", uval);
        } else {
            std::snprintf(buf, sizeof(buf), "0x%llx",
                          static_cast<unsigned long long>(intVal));
        }
        return std::string(buf);
    }

    /// Octal with 0 prefix: 8 -> "010"
    inline std::string transformOct(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;
        long long intVal = static_cast<long long>(pipeClampLL(numVal));
        char buf[64];
        if (intVal < 0) {
            unsigned long long uval = 0ULL - static_cast<unsigned long long>(intVal);
            std::snprintf(buf, sizeof(buf), "-0%llo", uval);
        } else if (intVal == 0) {
            return "0";
        } else {
            std::snprintf(buf, sizeof(buf), "0%llo",
                          static_cast<unsigned long long>(intVal));
        }
        return std::string(buf);
    }

    /// Binary with 0b prefix: 10 -> "0b1010"
    inline std::string transformBin(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;
        long long intVal = static_cast<long long>(pipeClampLL(numVal));
        bool negative = intVal < 0;
        unsigned long long uval = negative
            ? (0ULL - static_cast<unsigned long long>(intVal))
            : static_cast<unsigned long long>(intVal);

        if (uval == 0) return "0b0";

        std::string bits;
        unsigned long long tmp = uval;
        while (tmp > 0) {
            bits = static_cast<char>('0' + static_cast<int>(tmp & 1)) + bits;
            tmp >>= 1;
        }

        std::string result = negative ? "-0b" : "0b";
        result += bits;
        return result;
    }

    /// Human-readable bytes: 1048576 -> "1.0 MB"
    inline std::string transformBytes(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;

        static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
        static const int unitCount = 6;

        double absVal = std::fabs(numVal);
        double displayVal = absVal;
        int unitIdx = 0;

        while (displayVal >= 1024.0 && unitIdx < unitCount - 1) {
            displayVal /= 1024.0;
            ++unitIdx;
        }

        if (numVal < 0) displayVal = -displayVal;

        char buf[64];
        if (unitIdx == 0) {
            std::snprintf(buf, sizeof(buf), "%lld B",
                          static_cast<long long>(numVal));
        } else {
            std::snprintf(buf, sizeof(buf), "%.1f %s", displayVal,
                          units[unitIdx]);
        }
        return std::string(buf);
    }

    /// Human-readable time from milliseconds: 3661000 -> "1h 1m 1s"
    inline std::string transformDuration(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;

        long long totalMs = static_cast<long long>(pipeClampLL(numVal));
        bool negative = totalMs < 0;
        if (negative) {
            // Avoid signed negation UB on LLONG_MIN by using unsigned arithmetic
            unsigned long long uMs = static_cast<unsigned long long>(-(totalMs + 1)) + 1u;
            totalMs = static_cast<long long>(uMs);
        }

        long long totalSec = totalMs / 1000;
        long long ms = totalMs % 1000;
        long long hours = totalSec / 3600;
        long long remaining = totalSec % 3600;
        long long minutes = remaining / 60;
        long long seconds = remaining % 60;

        std::string result;
        if (negative) result += "-";

        if (totalSec == 0 && ms == 0) {
            return result + "0s";
        }

        if (totalSec == 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lldms", ms);
            return result + buf;
        }

        bool first = true;
        if (hours > 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lldh", hours);
            result += buf;
            first = false;
        }
        if (minutes > 0) {
            if (!first) result += " ";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lldm", minutes);
            result += buf;
            first = false;
        }
        if (seconds > 0 || (hours == 0 && minutes == 0)) {
            if (!first) result += " ";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%llds", seconds);
            result += buf;
        }

        return result;
    }

    /// Percentage: 0.856 -> "85.6%"
    inline std::string transformPct(const std::string &value) {
        double numVal;
        if (!pipeParseDouble(value, numVal)) return value;
        double pct = numVal * 100.0;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
        return std::string(buf);
    }

    // ----------------------------------------------------------------
    // Structural Transforms
    // ----------------------------------------------------------------

    /// Force JSON serialization of the value.
    inline std::string transformJson(const std::string &value) {
        if (value == "true" || value == "false") return value;
        if (value == "(null)") return "null";

        double numVal;
        if (pipeParseDouble(value, numVal)) {
            char buf[64];
            if (numVal == static_cast<double>(static_cast<long long>(numVal)) &&
                std::fabs(numVal) < 1e15) {
                std::snprintf(buf, sizeof(buf), "%lld",
                              static_cast<long long>(numVal));
            } else {
                std::snprintf(buf, sizeof(buf), "%.15g", numVal);
            }
            return std::string(buf);
        }

        // String — wrap in quotes with JSON escaping
        std::string result = "\"";
        for (size_t i = 0; i < value.size(); ++i) {
            char c = value[i];
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b";  break;
                case '\f': result += "\\f";  break;
                case '\n': result += "\\n";  break;
                case '\r': result += "\\r";  break;
                case '\t': result += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char esc[8];
                        std::snprintf(esc, sizeof(esc), "\\u%04x",
                                      static_cast<unsigned char>(c));
                        result += esc;
                    } else {
                        result += c;
                    }
            }
        }
        result += '"';
        return result;
    }

    /// Output the detected C++ type name.
    inline std::string transformType(const std::string &value) {
        if (value == "true" || value == "false") return "bool";
        if (value == "(null)") return "nullptr_t";

        double numVal;
        if (pipeParseDouble(value, numVal)) {
            if (value.find('.') == std::string::npos &&
                value.find('e') == std::string::npos &&
                value.find('E') == std::string::npos) {
                return "int";
            }
            return "double";
        }
        return "string";
    }

    // ----------------------------------------------------------------
    // Transform pipeline — apply left to right
    // ----------------------------------------------------------------

    /// Apply a sequence of transforms to a formatted value.
    /// Structural transforms (expand, str) are no-ops on the string —
    /// they affect property metadata and are handled in mapProperties.
    /// Unknown transforms pass the value through unchanged (fail-open).
    inline std::string applyTransforms(const std::string &value,
                                       const std::vector<Transform> &transforms) {
        if (transforms.empty()) return value;
        std::string result = value;
        for (size_t i = 0; i < transforms.size(); ++i) {
            const std::string &name = transforms[i].name;
            const std::string &arg  = transforms[i].arg;

            // Structural — skip (handled elsewhere)
            if (name == "expand" || name == "str") continue;

            // String transforms
            if      (name == "upper")    result = transformUpper(result);
            else if (name == "lower")    result = transformLower(result);
            else if (name == "trim")     result = transformTrim(result);
            else if (name == "truncate") result = transformTruncate(result, arg);
            else if (name == "pad")      result = transformPad(result, arg);
            else if (name == "padl")     result = transformPadLeft(result, arg);
            else if (name == "quote")    result = transformQuote(result);

            // Number transforms
            else if (name == "comma")    result = transformComma(result);
            else if (name == "hex")      result = transformHex(result);
            else if (name == "oct")      result = transformOct(result);
            else if (name == "bin")      result = transformBin(result);
            else if (name == "bytes")    result = transformBytes(result);
            else if (name == "duration") result = transformDuration(result);
            else if (name == "pct")      result = transformPct(result);

            // Structural value transforms
            else if (name == "json")     result = transformJson(result);
            else if (name == "type")     result = transformType(result);

            // Unknown transform — fail-open, value passes through
        }
        return result;
    }

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_PIPE_TRANSFORM_HPP
