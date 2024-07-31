# LunarLog

LunarLog is a flexible, high-performance C++ logging library designed for modern applications. It offers a range of features to meet various logging needs, from basic console output to structured JSON and XML logging.

## Features

- Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
- Support for [Message Templates](https://messagetemplates.org/) specification
- Customizable formatters, including built-in JSON and XML formatters
- Multiple transport options (file, console)
- Thread-safe design
- Rate limiting to prevent log flooding
- Placeholder validation for improved debugging
- Escaped brackets support for literal curly braces in log messages
- Context capture for enriched logging
- Compatible with C++11, C++14, and C++17

## Requirements

- C++11 or later compatible compiler

## Installation

1. Include the LunarLog headers in your project:

```cpp
#include "lunar_log.hpp"
```

2. Ensure all component headers are in your include path.
3. Configure your build system to use C++11 or later (e.g., `-std=c++11`, `-std=c++14`, or `-std=c++17` for GCC/Clang).

## Basic Usage

```cpp
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

    // Add a file sink with built-in XML formatter
    logger.addSink<minta::FileSink, minta::XmlFormatter>("app.xml.log");

    // Basic logging with named placeholders
    logger.info("User {username} logged in from {ip}", "alice", "192.168.1.1");

    // Demonstrating escaped brackets
    logger.info("Escaped brackets example: {{escaped}} {notEscaped}", "value");

    return 0;
}
```

## Advanced Features

### Custom Formatters

Create custom formatters by implementing the `IFormatter` interface:

```cpp
class CustomFormatter : public minta::IFormatter {
public:
    std::string format(const minta::LogEntry &entry) const override {
        return "CUSTOM: " + entry.message;
    }
};

// Usage
logger.addSink<minta::FileSink, CustomFormatter>("custom_log.txt");
```

### Rate Limiting

LunarLog automatically applies rate limiting to prevent log flooding:

```cpp
for (int i = 0; i < 2000; ++i) {
    logger.info("Rate limit test message {index}", i);
}
```

### Placeholder Validation

LunarLog provides warnings for common placeholder issues:

```cpp
logger.info("Empty placeholder: {}", "value");
logger.info("Repeated placeholder: {placeholder} and {placeholder}", "value1", "value2");
logger.info("Too few values: {placeholder1} and {placeholder2}", "value");
logger.info("Too many values: {placeholder}", "value1", "value2");
```

### Context Capture

Enrich your logs with contextual information:

```cpp
logger.setCaptureContext(true);
logger.setContext("session_id", "abc123");
logger.info("Log with global context");

{
    minta::ContextScope scope(logger, "request_id", "req456");
    logger.info("Log within scoped context");
}

logger.clearAllContext();

// Advanced usage with manual context specification
logger.logWithContext(minta::LogLevel::INFO, LUNAR_LOG_CONTEXT, "Manual context specification");

```

## Best Practices

1. Use named placeholders for better readability and maintainability.
2. Set an appropriate log level for production environments.
3. Implement multiple sinks for different logging needs (e.g., console, file, structured formats).
4. Use JSON or XML logging for easier log parsing and analysis.
5. Utilize custom formatters for specialized logging requirements.
6. Be aware of rate limiting when logging high-frequency events.
7. Use context capture to add rich, structured data to your logs.

## Compatibility

LunarLog is designed to work with C++11, C++14, and C++17. When compiling, specify the appropriate standard for your project.

## License

LunarLog is released under the MIT License. See the LICENSE file for details.

## Contributing

Contributions to LunarLog are welcome! Please feel free to submit a Pull Request.

## Support

If you encounter any issues or have questions about using LunarLog, please file an issue on the project's GitHub page.