// custom_formatter.cpp
//
// Demonstrates how to create a custom formatter by extending minta::IFormatter.
//
// A formatter converts a LogEntry into a string representation. LunarLog ships
// with HumanReadableFormatter, JsonFormatter, and XmlFormatter. You can create
// your own for CSV output, syslog format, or any other format.
//
// Compile: g++ -std=c++17 -I include examples/custom_formatter.cpp -o custom_formatter -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------------
// Example 1: CSV formatter
//
// Outputs log entries as comma-separated values, suitable for import
// into spreadsheets or log analysis tools.
// ---------------------------------------------------------------
class CsvFormatter : public minta::IFormatter {
public:
    std::string format(const minta::LogEntry& entry) const override {
        std::string ts = minta::detail::formatTimestamp(entry.timestamp);
        std::string level = minta::getLevelString(entry.level);
        // Use localizedMessage() to respect per-sink locale settings
        std::string msg = localizedMessage(entry);

        std::string csv;
        csv += escapeCsv(ts) + ",";
        csv += level + ",";
        csv += escapeCsv(msg);

        // Append properties as key=value pairs
        if (!entry.properties.empty()) {
            csv += ",";
            bool first = true;
            for (const auto& prop : entry.properties) {
                if (!first) csv += ";";
                csv += prop.name + "=" + prop.value;
                first = false;
            }
        }

        return csv;
    }

private:
    static std::string escapeCsv(const std::string& input) {
        // Wrap in quotes if the value contains commas, quotes, or newlines
        bool needsQuoting = false;
        for (char c : input) {
            if (c == ',' || c == '"' || c == '\n' || c == '\r') {
                needsQuoting = true;
                break;
            }
        }
        if (!needsQuoting) return input;

        std::string result = "\"";
        for (char c : input) {
            if (c == '"') result += "\"\"";  // escape quotes by doubling
            else result += c;
        }
        result += '"';
        return result;
    }
};

// ---------------------------------------------------------------
// Example 2: Compact single-line formatter
//
// Minimal format: LEVEL | message
// Useful for constrained environments or quick debugging.
// ---------------------------------------------------------------
class CompactFormatter : public minta::IFormatter {
public:
    std::string format(const minta::LogEntry& entry) const override {
        std::string result;
        // Pad level to 5 chars for alignment
        std::string level = minta::getLevelString(entry.level);
        while (level.size() < 5) level += ' ';

        result += level;
        result += " | ";
        result += localizedMessage(entry);

        // Append context if present
        if (!entry.customContext.empty()) {
            result += " [";
            bool first = true;
            for (const auto& ctx : entry.customContext) {
                if (!first) result += " ";
                result += ctx.first + "=" + ctx.second;
                first = false;
            }
            result += "]";
        }

        return result;
    }
};

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // ---------------------------------------------------------------
    // Use custom formatters with file sinks via addSink<Sink, Formatter>
    //
    // The two-template-parameter form of addSink creates the sink with
    // the given arguments and replaces its default formatter with the
    // custom one.
    // ---------------------------------------------------------------
    logger.addSink<minta::FileSink, CsvFormatter>("output.csv");
    logger.addSink<minta::FileSink, CompactFormatter>("output.compact.log");

    // ---------------------------------------------------------------
    // Log messages — each sink formats independently
    // ---------------------------------------------------------------
    logger.info("Application started on port {port}", 8080);
    logger.debug("Cache initialized with {size} entries", 256);
    logger.warn("Disk usage at {pct:P}", 0.89);
    logger.error("Failed to process request: {err}", "invalid JSON");

    // Context is included by formatters that support it
    logger.setContext("user", "alice");
    logger.info("User authenticated via {method}", "OAuth2");
    logger.clearAllContext();

    // Messages with special characters (CSV escaping test)
    logger.info("Message with comma, in it");
    logger.info("Message with \"quotes\" in it");

    // Flush and report
    logger.flush();

    std::cout << "\nCustom formatter examples complete."
              << "\nConsole output above uses the default HumanReadableFormatter."
              << "\nCheck output.csv for CSV-formatted output."
              << "\nCheck output.compact.log for compact-formatted output." << std::endl;
    return 0;
}
