#ifndef LUNAR_LOG_JSON_PARSER_HPP
#define LUNAR_LOG_JSON_PARSER_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <locale>

namespace minta {
namespace detail {

    /// Minimal JSON value type for parsing configuration files.
    ///
    /// Supports: null, bool, number, string, array, object.
    /// Header-only, no external dependencies.
    ///
    /// Memory layout: all type fields are stored simultaneously rather
    /// than in a tagged union.  A C++11 union with non-trivial members
    /// (string, vector, map) would require manual placement-new and
    /// destructor calls — error-prone for negligible gain on the small
    /// config files this parser handles.
    class JsonValue {
    public:
        enum Type { Null, Bool, Number, String, Array, Object };

        JsonValue() : m_type(Null), m_bool(false), m_number(0.0) {}

        Type type() const { return m_type; }
        bool isNull() const { return m_type == Null; }
        bool isBool() const { return m_type == Bool; }
        bool isNumber() const { return m_type == Number; }
        bool isString() const { return m_type == String; }
        bool isArray() const { return m_type == Array; }
        bool isObject() const { return m_type == Object; }

        bool asBool() const { return m_bool; }
        double asNumber() const { return m_number; }
        const std::string& asString() const { return m_string; }
        const std::vector<JsonValue>& asArray() const { return m_array; }
        const std::map<std::string, JsonValue>& asObject() const { return m_object; }

        bool hasKey(const std::string& key) const {
            return m_type == Object && m_object.count(key) > 0;
        }

        const JsonValue& operator[](const std::string& key) const {
            // Thread-safe: C++11 guarantees safe init of function-local
            // statics (§6.7/4); object is const after construction.
            static const JsonValue s_null;
            if (m_type != Object) return s_null;
            std::map<std::string, JsonValue>::const_iterator it = m_object.find(key);
            return it != m_object.end() ? it->second : s_null;
        }

        static JsonValue makeNull() { return JsonValue(); }

        static JsonValue makeBool(bool b) {
            JsonValue v;
            v.m_type = Bool;
            v.m_bool = b;
            return v;
        }

        static JsonValue makeNumber(double d) {
            JsonValue v;
            v.m_type = Number;
            v.m_number = d;
            return v;
        }

        static JsonValue makeString(const std::string& s) {
            JsonValue v;
            v.m_type = String;
            v.m_string = s;
            return v;
        }

        static JsonValue makeString(std::string&& s) {
            JsonValue v;
            v.m_type = String;
            v.m_string = std::move(s);
            return v;
        }

        static JsonValue makeArray(std::vector<JsonValue> arr) {
            JsonValue v;
            v.m_type = Array;
            v.m_array = std::move(arr);
            return v;
        }

        static JsonValue makeObject(std::map<std::string, JsonValue> obj) {
            JsonValue v;
            v.m_type = Object;
            v.m_object = std::move(obj);
            return v;
        }

        /// Parse a JSON string into a JsonValue tree.
        /// @throws std::runtime_error on parse failure.
        static JsonValue parse(const std::string& json);

    private:
        Type m_type;
        bool m_bool;
        double m_number;
        std::string m_string;
        std::vector<JsonValue> m_array;
        std::map<std::string, JsonValue> m_object;
    };

    // ----------------------------------------------------------------
    //  Recursive descent JSON parser
    // ----------------------------------------------------------------

    class JsonParser {
    public:
        explicit JsonParser(const std::string& input)
            : m_input(input), m_pos(0), m_depth(0) {}

        JsonValue parse() {
            skipWhitespace();
            if (m_pos >= m_input.size()) {
                throw std::runtime_error("JSON: empty input");
            }
            JsonValue result = parseValue();
            skipWhitespace();
            if (m_pos < m_input.size()) {
                throw std::runtime_error(
                    "JSON: unexpected trailing content at position " +
                    std::to_string(m_pos));
            }
            return result;
        }

    private:
        const std::string& m_input;
        size_t m_pos;
        int m_depth;

        static const int kMaxDepth = 128;

        struct DepthGuard {
            int& depth;
            DepthGuard(int& d, int max) : depth(d) {
                if (++depth > max) {
                    throw std::runtime_error(
                        "JSON: nesting depth exceeds maximum of " +
                        std::to_string(max));
                }
            }
            ~DepthGuard() { --depth; }
            DepthGuard(const DepthGuard&) = delete;
            DepthGuard& operator=(const DepthGuard&) = delete;
        };

        char peek() const {
            if (m_pos >= m_input.size()) {
                throw std::runtime_error("JSON: unexpected end of input");
            }
            return m_input[m_pos];
        }

        char advance() {
            char c = peek();
            ++m_pos;
            return c;
        }

        void expect(char c) {
            char got = advance();
            if (got != c) {
                std::string msg = "JSON: expected '";
                msg += c;
                msg += "' but got '";
                msg += got;
                msg += "' at position ";
                msg += std::to_string(m_pos - 1);
                throw std::runtime_error(msg);
            }
        }

        void skipWhitespace() {
            while (m_pos < m_input.size()) {
                char c = m_input[m_pos];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    ++m_pos;
                } else {
                    break;
                }
            }
        }

        JsonValue parseValue() {
            skipWhitespace();
            if (m_pos >= m_input.size()) {
                throw std::runtime_error("JSON: unexpected end of input");
            }
            char c = m_input[m_pos];
            switch (c) {
                case '"': return parseString();
                case '{': return parseObject();
                case '[': return parseArray();
                case 't': return parseLiteral("true", JsonValue::makeBool(true));
                case 'f': return parseLiteral("false", JsonValue::makeBool(false));
                case 'n': return parseLiteral("null", JsonValue::makeNull());
                default:
                    if (c == '-' || (c >= '0' && c <= '9')) {
                        return parseNumber();
                    }
                    throw std::runtime_error(
                        std::string("JSON: unexpected character '") + c +
                        "' at position " + std::to_string(m_pos));
            }
        }

        JsonValue parseString() {
            expect('"');
            std::string result;
            while (m_pos < m_input.size()) {
                char c = m_input[m_pos];
                if (c == '"') {
                    ++m_pos;
                    return JsonValue::makeString(std::move(result));
                }
                if (c == '\\') {
                    ++m_pos;
                    if (m_pos >= m_input.size()) {
                        throw std::runtime_error("JSON: unterminated escape sequence");
                    }
                    char esc = m_input[m_pos];
                    ++m_pos;
                    switch (esc) {
                        case '"':  result += '"'; break;
                        case '\\': result += '\\'; break;
                        case '/':  result += '/'; break;
                        case 'b':  result += '\b'; break;
                        case 'f':  result += '\f'; break;
                        case 'n':  result += '\n'; break;
                        case 'r':  result += '\r'; break;
                        case 't':  result += '\t'; break;
                        case 'u':  result += parseUnicodeEscape(); break;
                        default:
                            throw std::runtime_error(
                                std::string("JSON: invalid escape '\\") + esc + "'");
                    }
                } else {
                    // RFC 8259 §7: U+0000..U+001F must be escaped.
                    if (static_cast<unsigned char>(c) < 0x20) {
                        throw std::runtime_error(
                            "JSON: unescaped control character in string at "
                            "position " + std::to_string(m_pos));
                    }
                    result += c;
                    ++m_pos;
                }
            }
            throw std::runtime_error("JSON: unterminated string");
        }

        /// Parse a \\uXXXX escape sequence and return UTF-8 bytes.
        std::string parseUnicodeEscape() {
            if (m_pos + 4 > m_input.size()) {
                throw std::runtime_error("JSON: incomplete \\u escape");
            }
            unsigned codepoint = 0;
            for (int i = 0; i < 4; ++i) {
                char h = m_input[m_pos++];
                codepoint <<= 4;
                if (h >= '0' && h <= '9') codepoint |= (unsigned)(h - '0');
                else if (h >= 'a' && h <= 'f') codepoint |= (unsigned)(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') codepoint |= (unsigned)(h - 'A' + 10);
                else throw std::runtime_error(
                    std::string("JSON: invalid hex digit '") + h + "' in \\u escape");
            }
            // Handle surrogate pairs — reject lone surrogates
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (m_pos + 6 <= m_input.size() &&
                    m_input[m_pos] == '\\' && m_input[m_pos + 1] == 'u') {
                    m_pos += 2;
                    unsigned low = 0;
                    for (int i = 0; i < 4; ++i) {
                        char h = m_input[m_pos++];
                        low <<= 4;
                        if (h >= '0' && h <= '9') low |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') low |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') low |= (unsigned)(h - 'A' + 10);
                        else throw std::runtime_error("JSON: invalid hex in surrogate pair");
                    }
                    if (low < 0xDC00 || low > 0xDFFF) {
                        throw std::runtime_error(
                            "JSON: high surrogate not followed by low surrogate");
                    }
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                } else {
                    throw std::runtime_error(
                        "JSON: lone high surrogate in \\u escape");
                }
            } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                throw std::runtime_error(
                    "JSON: lone low surrogate in \\u escape");
            }
            return codepointToUtf8(codepoint);
        }

        static std::string codepointToUtf8(unsigned cp) {
            std::string result;
            if (cp < 0x80) {
                result += static_cast<char>(cp);
            } else if (cp < 0x800) {
                result += static_cast<char>(0xC0 | (cp >> 6));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                result += static_cast<char>(0xE0 | (cp >> 12));
                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x110000) {
                result += static_cast<char>(0xF0 | (cp >> 18));
                result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                throw std::runtime_error(
                    "JSON: codepoint out of Unicode range: U+" +
                    std::to_string(cp));
            }
            return result;
        }

        JsonValue parseNumber() {
            size_t start = m_pos;
            if (m_pos < m_input.size() && m_input[m_pos] == '-') ++m_pos;
            if (m_pos >= m_input.size() || m_input[m_pos] < '0' || m_input[m_pos] > '9') {
                throw std::runtime_error("JSON: invalid number at position " +
                    std::to_string(start));
            }
            if (m_input[m_pos] == '0') {
                ++m_pos;
            } else {
                while (m_pos < m_input.size() && m_input[m_pos] >= '0' && m_input[m_pos] <= '9') {
                    ++m_pos;
                }
            }
            if (m_pos < m_input.size() && m_input[m_pos] == '.') {
                ++m_pos;
                if (m_pos >= m_input.size() || m_input[m_pos] < '0' || m_input[m_pos] > '9') {
                    throw std::runtime_error("JSON: invalid decimal in number");
                }
                while (m_pos < m_input.size() && m_input[m_pos] >= '0' && m_input[m_pos] <= '9') {
                    ++m_pos;
                }
            }
            if (m_pos < m_input.size() && (m_input[m_pos] == 'e' || m_input[m_pos] == 'E')) {
                ++m_pos;
                if (m_pos < m_input.size() && (m_input[m_pos] == '+' || m_input[m_pos] == '-')) {
                    ++m_pos;
                }
                if (m_pos >= m_input.size() || m_input[m_pos] < '0' || m_input[m_pos] > '9') {
                    throw std::runtime_error("JSON: invalid exponent in number");
                }
                while (m_pos < m_input.size() && m_input[m_pos] >= '0' && m_input[m_pos] <= '9') {
                    ++m_pos;
                }
            }
            std::string numStr = m_input.substr(start, m_pos - start);
            // Locale-independent: strtod() uses the process locale which
            // may treat ',' as decimal separator, breaking "3.14" etc.
            std::istringstream iss(numStr);
            iss.imbue(std::locale::classic());
            double val = 0.0;
            iss >> val;
            if (iss.fail()) {
                // Out-of-range numbers (e.g. 1e309) set failbit but
                // produce infinity.  Accept them as valid JSON numbers
                // rather than rejecting the entire config file.
                // RFC 8259 §6 allows implementations to set limits on
                // numeric range; we choose to represent overflow as
                // infinity (matching nlohmann/json and RapidJSON).
                if (std::isinf(val)) {
                    iss.clear();
                } else {
                    throw std::runtime_error(
                        "JSON: failed to parse number: " + numStr);
                }
            }
            // Defensive: ensure the entire numeric string was consumed.
            // The structural validator above guarantees this, but the
            // check guards against future validator changes.
            iss >> std::ws;
            if (!iss.eof()) {
                throw std::runtime_error(
                    "JSON: unexpected trailing content in number: " +
                    numStr);
            }
            return JsonValue::makeNumber(val);
        }

        JsonValue parseObject() {
            DepthGuard guard(m_depth, kMaxDepth);
            expect('{');
            std::map<std::string, JsonValue> obj;
            skipWhitespace();
            if (m_pos < m_input.size() && m_input[m_pos] == '}') {
                ++m_pos;
                return JsonValue::makeObject(std::move(obj));
            }
            for (;;) {
                skipWhitespace();
                if (peek() != '"') {
                    throw std::runtime_error(
                        "JSON: expected string key at position " +
                        std::to_string(m_pos));
                }
                JsonValue keyVal = parseString();
                skipWhitespace();
                expect(':');
                JsonValue value = parseValue();
                obj[keyVal.asString()] = std::move(value);
                skipWhitespace();
                char c = peek();
                if (c == '}') {
                    ++m_pos;
                    return JsonValue::makeObject(std::move(obj));
                }
                if (c != ',') {
                    throw std::runtime_error(
                        "JSON: expected ',' or '}' in object at position " +
                        std::to_string(m_pos));
                }
                ++m_pos; // skip comma
            }
        }

        JsonValue parseArray() {
            DepthGuard guard(m_depth, kMaxDepth);
            expect('[');
            std::vector<JsonValue> arr;
            skipWhitespace();
            if (m_pos < m_input.size() && m_input[m_pos] == ']') {
                ++m_pos;
                return JsonValue::makeArray(std::move(arr));
            }
            for (;;) {
                arr.push_back(parseValue());
                skipWhitespace();
                char c = peek();
                if (c == ']') {
                    ++m_pos;
                    return JsonValue::makeArray(std::move(arr));
                }
                if (c != ',') {
                    throw std::runtime_error(
                        "JSON: expected ',' or ']' in array at position " +
                        std::to_string(m_pos));
                }
                ++m_pos; // skip comma
            }
        }

        JsonValue parseLiteral(const char* literal, const JsonValue& value) {
            size_t len = 0;
            while (literal[len]) ++len;
            if (m_pos + len > m_input.size()) {
                throw std::runtime_error(
                    std::string("JSON: unexpected end while parsing '") +
                    literal + "'");
            }
            for (size_t i = 0; i < len; ++i) {
                if (m_input[m_pos + i] != literal[i]) {
                    throw std::runtime_error(
                        std::string("JSON: invalid literal at position ") +
                        std::to_string(m_pos));
                }
            }
            m_pos += len;
            return value;
        }

    };

    inline JsonValue JsonValue::parse(const std::string& json) {
        JsonParser parser(json);
        return parser.parse();
    }

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_JSON_PARSER_HPP
