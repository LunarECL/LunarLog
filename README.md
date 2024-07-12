# LunarLog

LunarLog is a flexible, high-performance C++ logging library designed for modern applications. It offers a range of features to meet various logging needs, from basic console output to structured JSON logging.

## Features

- Header-only library for easy integration
- Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
- Support for [Message Templates](https://messagetemplates.org/) specification
- JSON logging support
- File rotation based on size
- Rate limiting to prevent log flooding
- Thread-safe design
- Separated source and sink components for greater flexibility
- Asynchronous logging with non-blocking log calls

## Requirements

- C++11 or later compatible compiler
- [nlohmann/json](https://github.com/nlohmann/json) library for JSON support (MIT License)

## Installation

Include the following header in your project:

```cpp
#include "lunar_log.hpp"
```

Ensure that all the component headers (lunar_log_common.hpp, lunar_log_sink_interface.hpp, etc.) are in your include path.

## Usage

```cpp
#include "lunar_log.hpp"

int main() {
    minta::LunarLog logger(minta::LogLevel::INFO);
    
    // Add a console sink using the factory pattern
    auto consoleSink = logger.addSink<minta::ConsoleSink>();

    // Add a file sink using the factory pattern
    auto fileSink = logger.addSink<minta::FileSink>("app.log", 10 * 1024 * 1024);

    logger.info("Application started");
    logger.warn("Low disk space: {remaining} MB remaining", 100);
    logger.error("Failed to connect to database: {error}", "Connection timeout");

    // Using escaped brackets
    logger.info("Escaped brackets example: {{escaped}} {not_escaped}", "value");

    return 0;
}
```

### JSON Logging

```cpp
minta::LunarLog logger(minta::LogLevel::DEBUG);
auto jsonFileSink = logger.addSink<minta::FileSink>("app.json.log", 10 * 1024 * 1024);
logger.enableJsonLogging(true);

logger.info("User {username} logged in from {ip_address}", "alice", "192.168.1.1");
```

This will produce a JSON log entry like:

```json
{
  "level": "INFO",
  "message": "User alice logged in from 192.168.1.1",
  "timestamp": "2024-07-09 14:04:56.519",
  "username": "alice",
  "ip_address": "192.168.1.1"
}
```

## Configuration

- Set minimum log level: `logger.setMinLevel(minta::LogLevel::WARN)`
- Enable/disable JSON logging: `logger.enableJsonLogging(true)`
- Add a sink: `logger.addSink<SinkType>(args...)`
- Remove a specific sink: `logger.removeSink(sinkPointer)`
- Remove all sinks of a specific type: `logger.removeSinkOfType<SinkType>()`

## Best Practices

1. Use named placeholders following the [Message Templates](https://messagetemplates.org/) specification for better readability and maintainability.
2. Set an appropriate log level for production environments.
3. Implement multiple sinks for different logging needs (e.g., file for persistent logs, console for immediate feedback).
4. Use JSON logging for easier log parsing and analysis.
5. Use escaped brackets when you need to include literal curly braces in your log messages.
6. Utilize the factory pattern for creating and managing sinks.

## License

LunarLog is released under the MIT License. See the LICENSE file for details.

## Contributing

Contributions to LunarLog are welcome! Please feel free to submit a Pull Request.

## Support

If you encounter any issues or have questions about using LunarLog, please file an issue on the project's GitHub page.
