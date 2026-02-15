#ifndef LUNAR_LOG_XML_FORMATTER_HPP
#define LUNAR_LOG_XML_FORMATTER_HPP

#include "formatter_interface.hpp"
#include "../core/log_common.hpp"
#include <sstream>
#include <iomanip>

namespace minta {
    class XmlFormatter : public IFormatter {
    public:
        std::string format(const LogEntry &entry) const override {
            std::ostringstream xml;
            xml << "<log_entry>";
            xml << "<level>" << getLevelString(entry.level) << "</level>";
            xml << "<timestamp>" << detail::formatTimestamp(entry.timestamp) << "</timestamp>";
            xml << "<message>" << escapeXmlString(entry.message) << "</message>";

            if (!entry.file.empty()) {
                xml << "<file>" << escapeXmlString(entry.file) << "</file>";
                xml << "<line>" << entry.line << "</line>";
                xml << "<function>" << escapeXmlString(entry.function) << "</function>";
            }

            if (!entry.customContext.empty()) {
                xml << "<context>";
                for (const auto &ctx : entry.customContext) {
                    std::string safeName = sanitizeXmlName(ctx.first);
                    xml << "<" << safeName << ">";
                    xml << escapeXmlString(ctx.second);
                    xml << "</" << safeName << ">";
                }
                xml << "</context>";
            }

            xml << "</log_entry>";
            return xml.str();
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