#ifndef LUNAR_LOG_OUTPUT_TEMPLATE_HPP
#define LUNAR_LOG_OUTPUT_TEMPLATE_HPP

#include "log_entry.hpp"
#include "log_common.hpp"
#include "log_level.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>

namespace minta {
namespace detail {

    /// Token types recognized by the output template parser.
    enum class OutputTokenType {
        Timestamp,       // {timestamp} or {timestamp:FMT}
        Level,           // {level}, {level:u3}, {level:l}
        Message,         // {message}
        Newline,         // {newline}
        Properties,      // {properties}
        Template,        // {template}
        Source,          // {source}
        ThreadId,        // {threadId}
        Exception        // {exception}
    };

    /// A compiled segment: either a literal string or a token with optional spec/alignment.
    struct OutputSegment {
        bool isLiteral;
        std::string literal;       // only when isLiteral == true
        OutputTokenType tokenType; // only when isLiteral == false
        std::string spec;          // e.g. "HH:mm:ss" for timestamp, "u3"/"l" for level
        int alignment;             // >0 right-align, <0 left-align, 0 = no alignment

        OutputSegment() : isLiteral(true), tokenType(OutputTokenType::Timestamp), alignment(0) {}

        static OutputSegment makeLiteral(const std::string& text) {
            OutputSegment seg;
            seg.isLiteral = true;
            seg.literal = text;
            // tokenType is irrelevant for literal segments; use default ctor value
            seg.tokenType = OutputTokenType::Timestamp;
            seg.alignment = 0;
            return seg;
        }

        static OutputSegment makeToken(OutputTokenType type, const std::string& spec = "", int align = 0) {
            OutputSegment seg;
            seg.isLiteral = false;
            seg.tokenType = type;
            seg.spec = spec;
            seg.alignment = align;
            return seg;
        }
    };

    /// Convert Serilog-style timestamp format tokens to strftime equivalents.
    /// yyyy->%Y, MM->%m, dd->%d, HH->%H, mm->%M, ss->%S
    /// fff is handled separately (milliseconds).
    inline std::string convertTimestampFormat(const std::string& fmt) {
        std::string result;
        result.reserve(fmt.size() + 8);
        size_t i = 0;
        while (i < fmt.size()) {
            // yyyy -> %Y
            if (i + 3 < fmt.size() && fmt[i] == 'y' && fmt[i+1] == 'y' && fmt[i+2] == 'y' && fmt[i+3] == 'y') {
                result += "%Y";
                i += 4;
            }
            // MM -> %m (month)
            else if (i + 1 < fmt.size() && fmt[i] == 'M' && fmt[i+1] == 'M') {
                result += "%m";
                i += 2;
            }
            // dd -> %d
            else if (i + 1 < fmt.size() && fmt[i] == 'd' && fmt[i+1] == 'd') {
                result += "%d";
                i += 2;
            }
            // HH -> %H
            else if (i + 1 < fmt.size() && fmt[i] == 'H' && fmt[i+1] == 'H') {
                result += "%H";
                i += 2;
            }
            // mm -> %M (minute) — lowercase mm
            else if (i + 1 < fmt.size() && fmt[i] == 'm' && fmt[i+1] == 'm') {
                result += "%M";
                i += 2;
            }
            // ss -> %S
            else if (i + 1 < fmt.size() && fmt[i] == 's' && fmt[i+1] == 's') {
                result += "%S";
                i += 2;
            }
            // fff -> milliseconds placeholder (replaced at render time)
            else if (i + 2 < fmt.size() && fmt[i] == 'f' && fmt[i+1] == 'f' && fmt[i+2] == 'f') {
                result += "\x01";  // sentinel for milliseconds
                i += 3;
            }
            else {
                result += fmt[i];
                ++i;
            }
        }
        return result;
    }

    /// Format a timestamp using a converted strftime pattern.
    /// Handles the \x01 sentinel for milliseconds (fff).
    /// Note: timestamps use local time via localtime_r/localtime_s.
    /// UTC output is not currently supported; add {timestamp:utc:FMT} in a future version.
    inline std::string formatTimestampWithPattern(const std::chrono::system_clock::time_point& tp, const std::string& pattern) {
        auto nowTime = std::chrono::system_clock::to_time_t(tp);
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

        std::tm tmBuf;
#if defined(_MSC_VER)
        localtime_s(&tmBuf, &nowTime);
#else
        localtime_r(&nowTime, &tmBuf);
#endif

        // Check if pattern contains millisecond sentinel
        size_t msPos = pattern.find('\x01');
        if (msPos == std::string::npos) {
            // 128 bytes sufficient for all standard strftime patterns.
            // strftime returns 0 if the output would not fit in the buffer,
            // in which case we return an empty string.
            char buf[128];
            size_t written = std::strftime(buf, sizeof(buf), pattern.c_str(), &tmBuf);
            if (written == 0) return std::string();
            return std::string(buf, written);
        }

        // Split around the sentinel, format each half, then join with ms
        std::string before = pattern.substr(0, msPos);
        std::string after = pattern.substr(msPos + 1);

        std::string result;
        if (!before.empty()) {
            char buf[128];
            size_t written = std::strftime(buf, sizeof(buf), before.c_str(), &tmBuf);
            if (written > 0) result.append(buf, written);
        }

        char msBuf[8];
        // Defensive: ensure non-negative milliseconds for pre-epoch time points
        std::snprintf(msBuf, sizeof(msBuf), "%03d", static_cast<int>((nowMs.count() + 1000) % 1000));
        result += msBuf;

        if (!after.empty()) {
            char buf[128];
            size_t written = std::strftime(buf, sizeof(buf), after.c_str(), &tmBuf);
            if (written > 0) result.append(buf, written);
        }

        return result;
    }

    /// Get a 3-char uppercase level abbreviation: TRC, DBG, INF, WRN, ERR, FTL
    inline const char* getLevelU3(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRC";
            case LogLevel::DEBUG: return "DBG";
            case LogLevel::INFO:  return "INF";
            case LogLevel::WARN:  return "WRN";
            case LogLevel::ERROR: return "ERR";
            case LogLevel::FATAL: return "FTL";
            default: return "UNK";
        }
    }

    /// Get lowercase full level name: trace, debug, info, warn, error, fatal
    inline const char* getLevelLower(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "trace";
            case LogLevel::DEBUG: return "debug";
            case LogLevel::INFO:  return "info";
            case LogLevel::WARN:  return "warn";
            case LogLevel::ERROR: return "error";
            case LogLevel::FATAL: return "fatal";
            default: return "unknown";
        }
    }

    /// Resolve a token name to its OutputTokenType.
    /// Returns true if recognized, false for unknown tokens.
    inline bool resolveTokenType(const std::string& name, OutputTokenType& out) {
        if (name == "timestamp")   { out = OutputTokenType::Timestamp;  return true; }
        if (name == "level")       { out = OutputTokenType::Level;      return true; }
        if (name == "message")     { out = OutputTokenType::Message;    return true; }
        if (name == "newline")     { out = OutputTokenType::Newline;    return true; }
        if (name == "properties")  { out = OutputTokenType::Properties; return true; }
        if (name == "template")    { out = OutputTokenType::Template;   return true; }
        if (name == "source")      { out = OutputTokenType::Source;     return true; }
        if (name == "threadId")    { out = OutputTokenType::ThreadId;   return true; }
        if (name == "exception")   { out = OutputTokenType::Exception;  return true; }
        return false;
    }

    /// Parse an output template string into compiled segments.
    ///
    /// Syntax:
    ///   {token}           — token with no spec
    ///   {token:spec}      — token with format spec
    ///   {token,N}         — right-aligned to width N
    ///   {token,-N}        — left-aligned to width N
    ///   {token,N:spec}    — alignment + spec
    ///   {{                — escaped literal {
    ///   }}                — escaped literal }
    ///   Unknown tokens    — produce empty string
    inline std::vector<OutputSegment> parseOutputTemplate(const std::string& templateStr) {
        std::vector<OutputSegment> segments;
        std::string literal;
        size_t i = 0;

        // char-by-char append; negligible cost since parse happens once at config time
        while (i < templateStr.size()) {
            if (templateStr[i] == '{') {
                // Escaped brace
                if (i + 1 < templateStr.size() && templateStr[i + 1] == '{') {
                    literal += '{';
                    i += 2;
                    continue;
                }
                // Token start
                size_t end = templateStr.find('}', i + 1);
                if (end == std::string::npos) {
                    // Unclosed brace — treat as literal
                    literal += templateStr[i];
                    ++i;
                    continue;
                }

                // Flush accumulated literal
                if (!literal.empty()) {
                    segments.push_back(OutputSegment::makeLiteral(literal));
                    literal.clear();
                }

                std::string content = templateStr.substr(i + 1, end - i - 1);
                i = end + 1;

                // Parse: name[,alignment][:spec]
                std::string name;
                std::string spec;
                int alignment = 0;

                // Find comma (alignment) and colon (spec)
                // Alignment comes before spec: {name,N:spec}
                size_t commaPos = content.find(',');
                size_t colonPos = std::string::npos;

                if (commaPos != std::string::npos) {
                    name = content.substr(0, commaPos);
                    std::string rest = content.substr(commaPos + 1);
                    // rest is "N:spec" or "N" or "-N:spec" or "-N"
                    colonPos = rest.find(':');
                    if (colonPos != std::string::npos) {
                        std::string alignStr = rest.substr(0, colonPos);
                        spec = rest.substr(colonPos + 1);
                        // Parse alignment (uses parseAlignment for consistent
                        // validation and MAX_ALIGNMENT_WIDTH clamping)
                        alignment = parseAlignment(alignStr);
                    } else {
                        // No spec, just alignment
                        alignment = parseAlignment(rest);
                    }
                } else {
                    // No comma — check for colon (spec only)
                    colonPos = content.find(':');
                    if (colonPos != std::string::npos) {
                        name = content.substr(0, colonPos);
                        spec = content.substr(colonPos + 1);
                    } else {
                        name = content;
                    }
                }

                OutputTokenType tokenType;
                if (resolveTokenType(name, tokenType)) {
                    // Pre-convert timestamp format at parse time
                    if (tokenType == OutputTokenType::Timestamp && !spec.empty()) {
                        spec = convertTimestampFormat(spec);
                    }
                    segments.push_back(OutputSegment::makeToken(tokenType, spec, alignment));
                } else {
                    // Unknown token → empty string (segment that renders to nothing)
                    // We still create a literal segment with empty text to represent it
                    segments.push_back(OutputSegment::makeLiteral(""));
                }
            } else if (templateStr[i] == '}') {
                // Escaped brace
                if (i + 1 < templateStr.size() && templateStr[i + 1] == '}') {
                    literal += '}';
                    i += 2;
                    continue;
                }
                // Unmatched } — treat as literal
                literal += templateStr[i];
                ++i;
            } else {
                literal += templateStr[i];
                ++i;
            }
        }

        // Flush remaining literal
        if (!literal.empty()) {
            segments.push_back(OutputSegment::makeLiteral(literal));
        }

        return segments;
    }

    /// A compiled output template — parse once, render many times.
    /// Thread-safe: the segments vector is immutable after construction.
    class OutputTemplate {
    public:
        OutputTemplate() {}

        explicit OutputTemplate(const std::string& templateStr)
            : m_segments(parseOutputTemplate(templateStr))
            , m_templateStr(templateStr) {}

        bool empty() const { return m_segments.empty(); }

        const std::string& templateString() const { return m_templateStr; }

        /// Render a log entry using the compiled template.
        /// If overrideMessage is non-empty, it replaces entry.message for
        /// the {message} token — used for per-sink locale re-rendering.
        std::string render(const LogEntry& entry, const std::string& overrideMessage = std::string()) const {
            std::string result;
            result.reserve(128);

            for (size_t i = 0; i < m_segments.size(); ++i) {
                const OutputSegment& seg = m_segments[i];
                if (seg.isLiteral) {
                    result += seg.literal;
                    continue;
                }

                std::string value;
                switch (seg.tokenType) {
                    case OutputTokenType::Timestamp:
                        value = renderTimestamp(entry, seg.spec);
                        break;
                    case OutputTokenType::Level:
                        value = renderLevel(entry.level, seg.spec);
                        break;
                    case OutputTokenType::Message:
                        value = overrideMessage.empty() ? entry.message : overrideMessage;
                        break;
                    case OutputTokenType::Newline:
                        value = "\n";
                        break;
                    case OutputTokenType::Properties:
                        value = renderProperties(entry);
                        break;
                    case OutputTokenType::Template:
                        value = entry.templateStr;
                        break;
                    case OutputTokenType::Source:
                        value = renderSource(entry);
                        break;
                    case OutputTokenType::ThreadId:
                        value = renderThreadId(entry);
                        break;
                    case OutputTokenType::Exception:
                        value = renderException(entry);
                        break;
                }

                if (seg.alignment != 0) {
                    value = applyAlignment(value, seg.alignment);
                }

                result += value;
            }

            return result;
        }

    private:
        std::vector<OutputSegment> m_segments;
        std::string m_templateStr;

        static std::string renderTimestamp(const LogEntry& entry, const std::string& spec) {
            if (spec.empty()) {
                return formatTimestamp(entry.timestamp);
            }
            return formatTimestampWithPattern(entry.timestamp, spec);
        }

        static std::string renderLevel(LogLevel level, const std::string& spec) {
            if (spec.empty()) {
                return getLevelString(level);
            }
            if (spec == "u3") {
                return getLevelU3(level);
            }
            if (spec == "l") {
                return getLevelLower(level);
            }
            // Unknown spec — return default
            return getLevelString(level);
        }

        static std::string renderProperties(const LogEntry& entry) {
            if (entry.customContext.empty()) return std::string();
            std::string result;
            bool first = true;
            for (const auto& ctx : entry.customContext) {
                if (!first) result += ", ";
                result += ctx.first;
                result += '=';
                result += ctx.second;
                first = false;
            }
            return result;
        }

        static std::string renderSource(const LogEntry& entry) {
            if (entry.file.empty()) return std::string();
            std::string result;
            result += entry.file;
            result += ':';
            result += std::to_string(entry.line);
            if (!entry.function.empty()) {
                result += ' ';
                result += entry.function;
            }
            return result;
        }

        static std::string renderThreadId(const LogEntry& entry) {
            std::ostringstream oss;
            oss << entry.threadId;
            return oss.str();
        }

        static std::string renderException(const LogEntry& entry) {
            if (!entry.hasException()) return std::string();
            std::string result;
            result += entry.exception->type;
            result += ": ";
            result += entry.exception->message;
            if (!entry.exception->chain.empty()) {
                size_t pos = 0;
                const std::string& chain = entry.exception->chain;
                while (pos < chain.size()) {
                    size_t nl = chain.find('\n', pos);
                    result += "\n  --- ";
                    if (nl == std::string::npos) {
                        result.append(chain, pos, chain.size() - pos);
                        break;
                    }
                    result.append(chain, pos, nl - pos);
                    pos = nl + 1;
                }
            }
            return result;
        }
    };

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_OUTPUT_TEMPLATE_HPP
