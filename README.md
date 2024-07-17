LunarLog
LunarLog is a flexible, high-performance C++ logging library designed for modern applications. It offers a range of features to meet various logging needs, from basic console output to structured JSON logging.

Features
Header-only library for easy integration
Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
Support for Message Templates specification
Customizable formatters, including built-in JSON formatter
Multiple transport options (file, console)
Thread-safe design
Rate limiting to prevent log flooding
Placeholder validation for improved debugging
Escaped brackets support for literal curly braces in log messages
Requirements
C++11 or later compatible compiler
nlohmann/json library for JSON support
Installation
Include the following header in your project:

#include "lunar_log.hpp"
Ensure that all the component headers are in your include path.

Basic Usage
#include "lunar_log.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
// Create a logger with minimum level set to TRACE
minta::LunarLog logger(minta::LogLevel::TRACE);

    // Add a console sink with default formatter
    logger.addSink<minta::ConsoleSink>();

    // Add a file sink with default formatter
    logger.addSink<minta::FileSink>("app.log");

    // Add a file sink with built-in JSON formatter
    logger.addSink<minta::FileSink, minta::JsonFormatter>("app.json.log");

    // Basic logging with named placeholders
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    // Demonstrating escaped brackets
    logger.info("Escaped brackets example: {{escaped}} {notEscaped}", "value");

    return 0;
}
Advanced Features
Custom Formatters
You can create custom formatters by implementing the IFormatter interface:

class CustomFormatter : public minta::IFormatter {
public:
std::string format(const minta::LogEntry &entry) const override {
return "CUSTOM: " + entry.message;
}
};

// Usage
logger.addSink<minta::FileSink, CustomFormatter>("custom_log.txt");
Rate Limiting
LunarLog automatically applies rate limiting to prevent log flooding:

for (int i = 0; i < 2000; ++i) {
logger.info("Rate limit test message {index}", i);
}
Placeholder Validation
LunarLog provides warnings for common placeholder issues:

logger.info("Empty placeholder: {}", "value");
logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");
logger.info("Too few values: {placeholder1} and {placeholder2}", "value");
logger.info("Too many values: {placeholder}", "value1", "value2");
Best Practices
Use named placeholders for better readability and maintainability.
Set an appropriate log level for production environments.
Implement multiple sinks for different logging needs.
Use JSON logging for easier log parsing and analysis.
Utilize custom formatters for specialized logging requirements.
Be aware of rate limiting when logging high-frequency events.
License
LunarLog is released under the MIT License. See the LICENSE file for details.

Contributing
Contributions to LunarLog are welcome! Please feel free to submit a Pull Request.

Support
If you encounter any issues or have questions about using LunarLog, please file an issue on the project's GitHub page.