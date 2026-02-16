#ifndef LUNAR_LOG_FILTER_RULE_HPP
#define LUNAR_LOG_FILTER_RULE_HPP

#include "log_level.hpp"
#include "log_entry.hpp"
#include <string>
#include <stdexcept>

namespace minta {

    /// A single DSL filter rule parsed from a string.
    ///
    /// Supported syntax:
    ///   level >= LEVEL  /  level == LEVEL  /  level != LEVEL
    ///   message contains 'text'
    ///   message startswith 'text'
    ///   context has 'key'
    ///   context key == 'value'
    ///   template == 'exact template'
    ///   template contains 'partial'
    ///   not <rule>
    ///
    /// Multiple rules are AND-combined externally (each must return true).
    class FilterRule {
    public:
        /// Parse a rule string into a FilterRule.
        /// Throws std::invalid_argument on unrecognized syntax.
        ///
        /// String values are delimited by outer single quotes with no escape
        /// sequences. Embedded quotes work by accident (outermost pair is
        /// stripped). There is no way to match a value that both starts and
        /// ends with a single quote.
        static FilterRule parse(const std::string& rule) {
            std::string trimmed = trim(rule);
            if (trimmed.empty()) {
                throw std::invalid_argument("Empty filter rule");
            }

            bool negated = false;
            if (startsWith(trimmed, "not ")) {
                negated = true;
                trimmed = trim(trimmed.substr(4));
                if (trimmed.empty()) {
                    throw std::invalid_argument("Empty rule after 'not'");
                }
            }

            FilterRule r;
            r.m_negated = negated;

            // level >= LEVEL  /  level == LEVEL  /  level != LEVEL
            if (startsWith(trimmed, "level ")) {
                std::string rest = trim(trimmed.substr(6));
                if (startsWith(rest, ">= ")) {
                    r.m_type = RuleType::LevelGe;
                    r.m_level = parseLevel(trim(rest.substr(3)));
                } else if (startsWith(rest, "== ")) {
                    r.m_type = RuleType::LevelEq;
                    r.m_level = parseLevel(trim(rest.substr(3)));
                } else if (startsWith(rest, "!= ")) {
                    r.m_type = RuleType::LevelNe;
                    r.m_level = parseLevel(trim(rest.substr(3)));
                } else {
                    throw std::invalid_argument("Invalid level operator in rule: " + rule);
                }
                return r;
            }

            // message contains 'text'  /  message startswith 'text'
            if (startsWith(trimmed, "message ")) {
                std::string rest = trim(trimmed.substr(8));
                if (startsWith(rest, "contains ")) {
                    r.m_type = RuleType::MessageContains;
                    r.m_value = extractQuoted(trim(rest.substr(9)), rule);
                } else if (startsWith(rest, "startswith ")) {
                    r.m_type = RuleType::MessageStartsWith;
                    r.m_value = extractQuoted(trim(rest.substr(11)), rule);
                } else {
                    throw std::invalid_argument("Invalid message operator in rule: " + rule);
                }
                return r;
            }

            // context has 'key'  /  context key == 'value'
            if (startsWith(trimmed, "context ")) {
                std::string rest = trim(trimmed.substr(8));
                if (startsWith(rest, "has ")) {
                    r.m_type = RuleType::ContextHas;
                    r.m_value = extractQuoted(trim(rest.substr(4)), rule);
                } else {
                    size_t spacePos = rest.find(' ');
                    if (spacePos == std::string::npos) {
                        throw std::invalid_argument("Invalid context rule: " + rule);
                    }
                    std::string key = rest.substr(0, spacePos);
                    std::string afterKey = trim(rest.substr(spacePos + 1));
                    if (startsWith(afterKey, "== ")) {
                        r.m_type = RuleType::ContextKeyEq;
                        r.m_key = key;
                        r.m_value = extractQuoted(trim(afterKey.substr(3)), rule);
                    } else {
                        throw std::invalid_argument("Invalid context operator in rule: " + rule);
                    }
                }
                return r;
            }

            // template == 'exact template'  /  template contains 'partial'
            if (startsWith(trimmed, "template ")) {
                std::string rest = trim(trimmed.substr(9));
                if (startsWith(rest, "== ")) {
                    r.m_type = RuleType::TemplateEq;
                    r.m_value = extractQuoted(trim(rest.substr(3)), rule);
                } else if (startsWith(rest, "contains ")) {
                    r.m_type = RuleType::TemplateContains;
                    r.m_value = extractQuoted(trim(rest.substr(9)), rule);
                } else {
                    throw std::invalid_argument("Invalid template operator in rule: " + rule);
                }
                return r;
            }

            throw std::invalid_argument("Unrecognized filter rule: " + rule);
        }

        /// Evaluate this rule against a log entry.
        /// Returns true if the entry passes (should be kept).
        bool evaluate(const LogEntry& entry) const {
            bool result = false;
            switch (m_type) {
                case RuleType::LevelGe:
                    result = entry.level >= m_level;
                    break;
                case RuleType::LevelEq:
                    result = entry.level == m_level;
                    break;
                case RuleType::LevelNe:
                    result = entry.level != m_level;
                    break;
                case RuleType::MessageContains:
                    result = entry.message.find(m_value) != std::string::npos;
                    break;
                case RuleType::MessageStartsWith:
                    result = entry.message.size() >= m_value.size() &&
                             entry.message.compare(0, m_value.size(), m_value) == 0;
                    break;
                case RuleType::ContextHas:
                    result = entry.customContext.count(m_value) > 0;
                    break;
                case RuleType::ContextKeyEq:
                    {
                        auto it = entry.customContext.find(m_key);
                        result = it != entry.customContext.end() && it->second == m_value;
                    }
                    break;
                case RuleType::TemplateEq:
                    result = entry.templateStr == m_value;
                    break;
                case RuleType::TemplateContains:
                    result = entry.templateStr.find(m_value) != std::string::npos;
                    break;
                default:
                    break;
            }
            return m_negated ? !result : result;
        }

    private:
        enum class RuleType {
            LevelGe,
            LevelEq,
            LevelNe,
            MessageContains,
            MessageStartsWith,
            ContextHas,
            ContextKeyEq,
            TemplateEq,
            TemplateContains
        };

        RuleType m_type;
        bool m_negated;
        LogLevel m_level;
        std::string m_value;
        std::string m_key;

        FilterRule() : m_type(RuleType::LevelGe), m_negated(false), m_level(LogLevel::TRACE) {}

        static LogLevel parseLevel(const std::string& s) {
            if (s == "TRACE") return LogLevel::TRACE;
            if (s == "DEBUG") return LogLevel::DEBUG;
            if (s == "INFO")  return LogLevel::INFO;
            if (s == "WARN")  return LogLevel::WARN;
            if (s == "ERROR") return LogLevel::ERROR;
            if (s == "FATAL") return LogLevel::FATAL;
            throw std::invalid_argument("Unknown log level: " + s);
        }

        /// Extract a single-quoted string value, e.g. 'hello world'.
        static std::string extractQuoted(const std::string& s, const std::string& rule) {
            if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
                return s.substr(1, s.size() - 2);
            }
            throw std::invalid_argument("Expected single-quoted string in rule: " + rule);
        }

        static std::string trim(const std::string& s) {
            size_t start = 0;
            while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) ++start;
            size_t end = s.size();
            while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) --end;
            return s.substr(start, end - start);
        }

        static bool startsWith(const std::string& s, const std::string& prefix) {
            return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
        }
    };

} // namespace minta

#endif // LUNAR_LOG_FILTER_RULE_HPP
