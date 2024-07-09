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

## Requirements

- C++17 compatible compiler
- [nlohmann/json](https://github.com/nlohmann/json) library for JSON support (MIT License)

## Installation

1. **Using the single include version with JSON included**:
   - Include the `single_include/lunar_log.hpp` file in your project.
   
   ```cpp
   #include "single_include/lunar_log.hpp"
   ```

2. **Using the separate include version**:
    - Include the `include/lunar_log.hpp` and ensure you have the `nlohmann/json.hpp` file included and properly linked.

   ```cpp
   #include "include/lunar_log.hpp"
   ```

## Usage

```cpp
#include "lunar_log.hpp"

int main() {
    minta::LunarLog logger(minta::LunarLog::Level::INFO, "app.log");

    logger.info("Application started");
    logger.warn("Low disk space: {remaining} MB remaining", 100);
    logger.error("Failed to connect to database: {error}", "Connection timeout");

    return 0;
}
```

### JSON Logging

```cpp
minta::LunarLog logger(minta::LunarLog::Level::DEBUG, "app.json.log", 10 * 1024 * 1024, true);

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

- Set minimum log level: `logger.setMinLevel(minta::LunarLog::Level::WARN)`
- Enable/disable JSON logging: `logger.enableJsonLogging(true)`
- Set log file: `logger.setLogFile("new_log_file.log")`

## Best Practices

1. Use named placeholders following the [Message Templates](https://messagetemplates.org/) specification for better readability and maintainability.
2. Set an appropriate log level for production environments.
3. Monitor log file sizes and implement log rotation as needed.
4. Use JSON logging for easier log parsing and analysis.

## License

LunarLog is released under the MIT License. See the LICENSE file for details.

```
MIT License

Copyright (c) 2024 Minseok Kim

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Contributing

Contributions to LunarLog are welcome! Please feel free to submit a Pull Request.

## Support

If you encounter any issues or have questions about using LunarLog, please file an issue on the project's GitHub page.

