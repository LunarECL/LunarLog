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
            xml << "<timestamp>" << formatTimestamp(entry.timestamp) << "</timestamp>";
            xml << "<message>" << escapeXmlString(entry.message) << "</message>";

            for (const auto &arg : entry.arguments) {
                xml << "<argument name=\"" << escapeXmlString(arg.first) << "\">";
                xml << escapeXmlString(arg.second) << "</argument>";
            }

            xml << "</log_entry>";
            return xml.str();
        }

    private:
        static std::string escapeXmlString(const std::string &input) {
            std::ostringstream result;
            for (char c : input) {
                switch (c) {
                    case '<': result << "&lt;"; break;
                    case '>': result << "&gt;"; break;
                    case '&': result << "&amp;"; break;
                    case '\'': result << "&apos;"; break;
                    case '"': result << "&quot;"; break;
                    default: result << c; break;
                }
            }
            return result.str();
        }
    };
} // namespace minta

#endif // LUNAR_LOG_XML_FORMATTER_HPP
