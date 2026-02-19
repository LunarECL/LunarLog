#ifndef LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP
#define LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include "../core/output_template.hpp"
#include <string>

namespace minta {
    class HumanReadableFormatter : public IFormatter {
    public:
        /// Set the output template for this formatter.
        /// Must be called before logging begins (same contract as
        /// useFormatter / addSink). Not safe to call concurrently
        /// with format().
        void setOutputTemplate(const std::string& templateStr) {
            if (templateStr.empty()) {
                m_outputTemplate = detail::OutputTemplate();
                m_hasTemplate = false;
            } else {
                m_outputTemplate = detail::OutputTemplate(templateStr);
                m_hasTemplate = true;
            }
        }

        std::string format(const LogEntry &entry) const override {
            if (m_hasTemplate) {
                return m_outputTemplate.render(entry, localizedMessage(entry));
            }
            return formatDefault(entry);
        }

    private:
        detail::OutputTemplate m_outputTemplate;
        bool m_hasTemplate = false;

        std::string formatDefault(const LogEntry &entry) const {
            std::string result;
            result.reserve(80 + entry.message.size() + entry.file.size() + entry.function.size());
            result += detail::formatTimestamp(entry.timestamp);
            result += " [";
            result += getLevelString(entry.level);
            result += "] ";
            result += localizedMessage(entry);

            if (!entry.file.empty()) {
                result += " [";
                result += entry.file;
                result += ':';
                result += std::to_string(entry.line);
                result += ' ';
                result += entry.function;
                result += ']';
            }

            if (!entry.customContext.empty()) {
                result += " {";
                bool first = true;
                for (const auto &ctx : entry.customContext) {
                    if (!first) result += ", ";
                    result += ctx.first;
                    result += '=';
                    if (ctx.second.find(',') != std::string::npos ||
                        ctx.second.find('=') != std::string::npos ||
                        ctx.second.find('"') != std::string::npos) {
                        result += '"';
                        for (char c : ctx.second) {
                            if (c == '"') result += '\\';
                            result += c;
                        }
                        result += '"';
                    } else {
                        result += ctx.second;
                    }
                    first = false;
                }
                result += '}';
            }

            if (entry.hasException()) {
                result += "\n  ";
                result += entry.exception->type;
                result += ": ";
                result += entry.exception->message;
                if (!entry.exception->chain.empty()) {
                    const std::string& chain = entry.exception->chain;
                    size_t pos = 0;
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
            }

            return result;
        }
    };
} // namespace minta

#endif // LUNAR_LOG_HUMAN_READABLE_FORMATTER_HPP