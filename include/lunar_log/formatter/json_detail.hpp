#ifndef LUNAR_LOG_JSON_DETAIL_HPP
#define LUNAR_LOG_JSON_DETAIL_HPP

#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cmath>

// Platform-specific includes for locale-independent strtod.
// strtod_l / _strtod_l lets us parse with the "C" locale regardless
// of the process-wide locale set via setlocale().
#if defined(_MSC_VER)
    #include <locale.h>
#elif defined(__APPLE__)
    #include <xlocale.h>
#elif defined(__GLIBC__) || defined(__FreeBSD__)
    #include <locale.h>
#endif

namespace minta {
namespace detail {
namespace json {

    /// Locale-independent strtod.  Prevents the global C locale from
    /// interfering with JSON number parsing (e.g. locales that use ','
    /// as the decimal separator would cause plain strtod to reject "3.14").
    ///
    /// Uses strtod_l / _strtod_l where available (glibc, macOS, MSVC).
    /// Falls back to plain strtod on other platforms -- the output-side
    /// comma-to-dot replacement still produces valid JSON, but '.' in
    /// the *input* may be misinterpreted under a non-"C" locale on those
    /// platforms.
    inline double strtodLocaleIndependent(const char* str, char** endptr) {
#if defined(_MSC_VER)
        static _locale_t c_locale = _create_locale(_LC_ALL, "C");
        return _strtod_l(str, endptr, c_locale);
#elif defined(__GLIBC__) || defined(__APPLE__) || defined(__FreeBSD__)
        static locale_t c_locale = newlocale(LC_ALL_MASK, "C", (locale_t)0);
        return strtod_l(str, endptr, c_locale);
#else
        return std::strtod(str, endptr);
#endif
    }

    /// Escape a string for inclusion in JSON output.
    /// Handles special characters (quotes, backslash, control chars).
    /// UTF-8 multi-byte sequences (>= 0x80) pass through unescaped.
    inline std::string escapeJsonString(const std::string &input) {
        std::string result;
        result.reserve(input.size());
        for (char c : input) {
            switch (c) {
                case '"': result += R"(\")"; break;
                case '\\': result += R"(\\)"; break;
                case '\b': result += R"(\b)"; break;
                case '\f': result += R"(\f)"; break;
                case '\n': result += R"(\n)"; break;
                case '\r': result += R"(\r)"; break;
                case '\t': result += R"(\t)"; break;
                default:
                    if ('\x00' <= c && c <= '\x1f') {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                     static_cast<unsigned char>(c));
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }

    /// Attempt to emit a JSON-native value for @ (destructure) properties.
    /// Values arrive as strings (post-toString conversion), so original type
    /// info is lost.  This function uses string-based heuristics: "true"/"false"
    /// become JSON booleans, numeric-looking strings become JSON numbers.
    /// Known limitation: a string argument "true" becomes boolean true, and
    /// "3.14" becomes number 3.14.  Use the $ (stringify) operator to force
    /// string representation when this coercion is undesirable.
    /// Note: nullptr/"(null)" is emitted as the string "(null)", not JSON null,
    /// since the MessageTemplates spec does not mandate null handling.
    inline std::string toJsonNativeValue(const std::string &value) {
        if (value == "true" || value == "false") {
            return value;
        }

        if (value.empty()) {
            return "\"\"";
        }

        const char* start = value.c_str();
        char* end = nullptr;
        errno = 0;
        double numVal = strtodLocaleIndependent(start, &end);
        if (errno == 0 && end != start
            && static_cast<size_t>(end - start) == value.size()
            && std::isfinite(numVal)) {
            // Re-serialize from the parsed double to guarantee valid JSON.
            // strtod accepts inputs like "+42", " 42", "0x1A" which are
            // NOT valid JSON numbers, so we cannot return the original string.
            char buf[64];
            if (numVal == static_cast<double>(static_cast<long long>(numVal))
                && std::fabs(numVal) < 1e15) {
                std::snprintf(buf, sizeof(buf), "%lld",
                             static_cast<long long>(numVal));
            } else {
                std::snprintf(buf, sizeof(buf), "%.15g", numVal);
            }
            // Locale-safety: some locales use ',' as decimal separator.
            // Replace with '.' to guarantee valid JSON numbers.
            for (char *p = buf; *p; ++p) {
                if (*p == ',') *p = '.';
            }
            return std::string(buf);
        }
        errno = 0;

        std::string result = "\"";
        result += escapeJsonString(value);
        result += '"';
        return result;
    }

} // namespace json
} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_JSON_DETAIL_HPP
