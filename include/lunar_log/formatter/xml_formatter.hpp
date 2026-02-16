#ifndef LUNAR_LOG_XML_FORMATTER_HPP
#define LUNAR_LOG_XML_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <string>
#include <cstdio>

namespace minta {
    class XmlFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::string xml;
            xml += "<log_entry>";
            xml += "<level>";
            xml += getLevelString(entry.level);
            xml += "</level>";
            xml += "<timestamp>";
            xml += detail::formatTimestamp(entry.timestamp);
            xml += "</timestamp>";
            xml += "<message>";
            xml += escapeXmlString(localizedMessage(entry));
            xml += "</message>";

            if (!entry.templateStr.empty()) {
                xml += "<MessageTemplate hash=\"";
                xml += detail::toHexString(entry.templateHash);
                xml += "\">";
                xml += escapeXmlString(entry.templateStr);
                xml += "</MessageTemplate>";
            }

            if (!entry.file.empty()) {
                xml += "<file>";
                xml += escapeXmlString(entry.file);
                xml += "</file>";
                xml += "<line>";
                xml += std::to_string(entry.line);
                xml += "</line>";
                xml += "<function>";
                xml += escapeXmlString(entry.function);
                xml += "</function>";
            }

            if (!entry.customContext.empty()) {
                xml += "<context>";
                for (const auto &ctx : entry.customContext) {
                    std::string safeName = sanitizeXmlName(ctx.first);
                    xml += "<";
                    xml += safeName;
                    xml += ">";
                    xml += escapeXmlString(ctx.second);
                    xml += "</";
                    xml += safeName;
                    xml += ">";
                }
                xml += "</context>";
            }

            if (!entry.properties.empty()) {
                xml += "<properties>";
                for (const auto &prop : entry.properties) {
                    std::string safeName = sanitizeXmlName(prop.name);
                    xml += "<";
                    xml += safeName;
                    if (prop.op == '@') {
                        xml += " destructure=\"true\"";
                    } else if (prop.op == '$') {
                        xml += " stringify=\"true\"";
                    }
                    xml += ">";
                    xml += escapeXmlString(prop.value);
                    xml += "</";
                    xml += safeName;
                    xml += ">";
                }
                xml += "</properties>";
            }

            xml += "</log_entry>";
            return xml;
        }

    private:
        static std::string sanitizeXmlName(const std::string &input) {
            if (input.empty()) return "_";
            std::string result;
            result.reserve(input.size());
            for (size_t i = 0; i < input.size(); ++i) {
                char c = input[i];
                bool valid = (c == '_' || c == ':') ||
                             (c >= 'A' && c <= 'Z') ||
                             (c >= 'a' && c <= 'z') ||
                             (i > 0 && ((c >= '0' && c <= '9') || c == '-' || c == '.'));
                result += valid ? c : '_';
            }
            // Safety net: the loop above already replaces invalid start chars with '_',
            // so this check for a leading digit/dash/dot is unreachable with the
            // current logic. Kept as defensive validation in case the loop changes.
            if (result.empty() || result[0] == '-' || result[0] == '.' || (result[0] >= '0' && result[0] <= '9')) {
                result.insert(result.begin(), '_');
            }
            return result;
        }

        static std::string escapeXmlString(const std::string &input) {
            std::string result;
            result.reserve(input.size());
            for (char c : input) {
                unsigned char uc = static_cast<unsigned char>(c);
                if (uc < 0x20 && uc != 0x09 && uc != 0x0A && uc != 0x0D) {
                    result += ' ';
                    continue;
                }
                switch (c) {
                    case '<': result += "&lt;"; break;
                    case '>': result += "&gt;"; break;
                    case '&': result += "&amp;"; break;
                    case '\'': result += "&apos;"; break;
                    case '"': result += "&quot;"; break;
                    default: result += c; break;
                }
            }
            return result;
        }
    };
} // namespace minta

#endif // LUNAR_LOG_XML_FORMATTER_HPP