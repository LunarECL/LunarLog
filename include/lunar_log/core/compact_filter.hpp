#ifndef LUNAR_LOG_COMPACT_FILTER_HPP
#define LUNAR_LOG_COMPACT_FILTER_HPP

#include "filter_rule.hpp"
#include "log_level.hpp"
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>

namespace minta {
namespace detail {

    inline std::string compactToUpper(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
        }
        return result;
    }

    inline std::string compactStripQuotes(const std::string& s) {
        if (s.size() >= 2) {
            char first = s.front();
            char last = s.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                return s.substr(1, s.size() - 2);
            }
        }
        return s;
    }

    /// Wrap a value in single quotes for DSL consumption.
    /// Throws if the value contains a single quote (cannot be safely
    /// represented in the DSL's outermost-quote-stripping parser).
    inline std::string compactDslQuote(const std::string& s) {
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\'') {
                throw std::invalid_argument(
                    "Compact filter value cannot contain single quotes (DSL limitation). "
                    "Use addFilterRule() or setFilter() predicate instead. Value: " + s);
            }
        }
        return "'" + s + "'";
    }

    inline bool compactIsLevelName(const std::string& upper) {
        return upper == "TRACE" || upper == "DEBUG" || upper == "INFO" ||
               upper == "WARN"  || upper == "WARNING" || upper == "ERROR" || upper == "FATAL";
    }

    inline FilterRule parseCompactToken(const std::string& token) {
        if (token.empty()) {
            throw std::invalid_argument("Empty compact filter token");
        }

        if (token.size() >= 2 && token.back() == '+') {
            std::string levelStr = compactToUpper(token.substr(0, token.size() - 1));
            if (compactIsLevelName(levelStr)) {
                if (levelStr == "WARNING") levelStr = "WARN";
                return FilterRule::parse("level >= " + levelStr);
            }
        }

        if (token.size() > 5 && token[0] == '!' && token[1] == 't' &&
            token[2] == 'p' && token[3] == 'l' && token[4] == ':') {
            std::string pattern = compactStripQuotes(token.substr(5));
            return FilterRule::parse("not template == " + compactDslQuote(pattern));
        }

        if (token.size() > 4 && token[0] == 't' && token[1] == 'p' &&
            token[2] == 'l' && token[3] == ':') {
            std::string pattern = compactStripQuotes(token.substr(4));
            return FilterRule::parse("template == " + compactDslQuote(pattern));
        }

        if (token.size() > 2 && token[0] == '!' && token[1] == '~') {
            std::string keyword = compactStripQuotes(token.substr(2));
            if (keyword.empty()) {
                throw std::invalid_argument("Empty keyword in compact filter: " + token);
            }
            return FilterRule::parse("not message contains " + compactDslQuote(keyword));
        }

        if (token.size() > 1 && token[0] == '~') {
            std::string keyword = compactStripQuotes(token.substr(1));
            if (keyword.empty()) {
                throw std::invalid_argument("Empty keyword in compact filter: " + token);
            }
            return FilterRule::parse("message contains " + compactDslQuote(keyword));
        }

        if (token.size() > 4 && token[0] == 'c' && token[1] == 't' &&
            token[2] == 'x' && token[3] == ':') {
            std::string rest = token.substr(4);
            size_t eqPos = std::string::npos;
            bool inQuote = false;
            char quoteChar = 0;
            for (size_t j = 0; j < rest.size(); ++j) {
                if (!inQuote && (rest[j] == '"' || rest[j] == '\'')) {
                    inQuote = true;
                    quoteChar = rest[j];
                } else if (inQuote && rest[j] == quoteChar) {
                    inQuote = false;
                } else if (!inQuote && rest[j] == '=') {
                    eqPos = j;
                    break;
                }
            }

            if (eqPos != std::string::npos && eqPos > 0) {
                std::string key = compactStripQuotes(rest.substr(0, eqPos));
                std::string val = compactStripQuotes(rest.substr(eqPos + 1));
                if (key.empty()) {
                    throw std::invalid_argument("Empty context key in compact filter: " + token);
                }
                if (val.empty()) {
                    throw std::invalid_argument("Empty context value in compact filter: " + token);
                }
                return FilterRule::parse("context " + key + " == " + compactDslQuote(val));
            } else {
                std::string key = compactStripQuotes(rest);
                return FilterRule::parse("context has " + compactDslQuote(key));
            }
        }

        // Catch bare prefixes that fall through because size guards above
        // use strict > (e.g., token.size() > 4 for ctx:). Keep in sync.
        if (token == "ctx:" || token == "tpl:" || token == "!tpl:") {
            throw std::invalid_argument("Missing value after '" + token + "' in compact filter");
        }
        throw std::invalid_argument("Unrecognized compact filter token: " + token);
    }

    /// Parse a compact filter expression string into FilterRule objects.
    /// Tokens are space-separated and AND-combined.
    /// Syntax: LEVEL+, ~keyword, !~keyword, ctx:key, ctx:key=val, tpl:pattern, !tpl:pattern
    /// Level names are case-insensitive. Keywords are case-sensitive.
    inline std::vector<FilterRule> parseCompactFilter(const std::string& expr) {
        std::vector<FilterRule> rules;
        if (expr.empty()) return rules;

        std::vector<std::string> tokens;
        size_t i = 0;
        while (i < expr.size()) {
            while (i < expr.size() && (expr[i] == ' ' || expr[i] == '\t')) ++i;
            if (i >= expr.size()) break;

            std::string token;
            while (i < expr.size() && expr[i] != ' ' && expr[i] != '\t') {
                if (expr[i] == '"' || expr[i] == '\'') {
                    char quote = expr[i];
                    token += quote;
                    ++i;
                    while (i < expr.size() && expr[i] != quote) {
                        token += expr[i];
                        ++i;
                    }
                    if (i >= expr.size()) {
                        throw std::invalid_argument(
                            "Unterminated quote in compact filter expression");
                    }
                    token += expr[i];
                    ++i;
                } else {
                    token += expr[i];
                    ++i;
                }
            }
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        for (size_t t = 0; t < tokens.size(); ++t) {
            rules.push_back(parseCompactToken(tokens[t]));
        }

        return rules;
    }

} // namespace detail
} // namespace minta

#endif // LUNAR_LOG_COMPACT_FILTER_HPP
