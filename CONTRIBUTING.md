# Contributing to LunarLog

## Building

### Prerequisites

- CMake 3.10+
- C++ compiler with C++11 support (GCC, Clang, MSVC)
- GoogleTest is fetched automatically by CMake

### Build Steps

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DLUNARLOG_CXX_STANDARD=17
cmake --build build
```

### Running Tests

```bash
cd build && ctest --output-on-failure
```

### Building Examples Only

```bash
cmake -B build -DLUNARLOG_BUILD_TESTS=OFF
cmake --build build
```

## Code Style

- **Header-only**: all library code lives in `.hpp` files under `include/`.
- **C++11 compatibility**: library code must compile with C++11. Use `minta::detail::make_unique` instead of `std::make_unique`. Tests may use C++17 features (e.g. `<filesystem>`).
- **Namespace**: all public types live in `minta`. Internal helpers live in `minta::detail`.
- **Include guards**: use `#ifndef LUNAR_LOG_*_HPP` / `#define` / `#endif` (not `#pragma once`).
- **Naming**:
  - Classes and structs: `PascalCase` (`LogEntry`, `FileSink`)
  - Methods and functions: `camelCase` (`setMinLevel`, `addSink`)
  - Member variables: `m_camelCase` (`m_logQueue`, `m_isRunning`)
  - Constants: `kPascalCase` (`kRateLimitMaxLogs`)
  - Enums: `PascalCase` values (`LogLevel::TRACE`)
- **Braces**: opening brace on same line for functions and control flow.
- **Thread safety**: document the thread-safety contract for any new public method. Use `std::atomic` for flags, `std::mutex` for shared state.

## Pull Request Process

1. Fork the repository and create a feature branch from `main`.
2. Make your changes. Keep commits focused — one logical change per commit.
3. Add tests for all new code. Ensure existing tests still pass.
4. Update documentation (README, examples) if the change affects the public API.
5. Run the full build and test suite before submitting.
6. Open a pull request with a clear description of what changed and why.

## Adding New Features

### Adding a New Formatter

1. Create `include/lunar_log/formatter/your_formatter.hpp`.
2. Extend `IFormatter` and implement `format(const LogEntry&) const`.
3. Call `localizedMessage(entry)` instead of `entry.message` to support per-sink locale.
4. Add `#include` to `include/lunar_log.hpp`.
5. Add tests in `test/tests/`.
6. Update `tools/generate_single_header.py` if the file should be in the single-header build.

### Adding a New Sink

1. Create `include/lunar_log/sink/your_sink.hpp`.
2. Extend `ISink` (custom write logic) or `BaseSink` (formatter + transport pattern).
3. For `ISink`: implement `write(const LogEntry&)` and call `setFormatter()` in the constructor.
4. For `BaseSink`: call `setFormatter()` and `setTransport()` in the constructor.
5. Add `#include` to `include/lunar_log.hpp`.
6. Add tests.

### Adding a New Format Specifier

1. Add parsing logic to `detail::applyFormat()` in `include/lunar_log/core/log_common.hpp`.
2. Add tests in `test/tests/test_suffix_formatting.cpp`.
3. Document the new specifier in `README.md`.

### Adding a New Transport

1. Create `include/lunar_log/transport/your_transport.hpp`.
2. Extend `ITransport` and implement `write(const std::string&)`.
3. Add `#include` to `include/lunar_log.hpp`.
4. Add tests.

## Test Requirements

- All new code must have corresponding tests.
- Tests use [GoogleTest](https://github.com/google/googletest).
- Test files go in `test/tests/`. Shared utilities go in `test/tests/utils/`.
- Test helpers (e.g. `StringSink` for capturing output) are in `test/tests/utils/test_utils.hpp`.
- Run the full suite (`ctest --output-on-failure`) before submitting a PR.

## Commit Messages

- Use imperative mood: "Add feature" not "Added feature".
- Keep the first line under 72 characters.
- Focus on *what* the change does and *why*, not *how*.
- Be concise and direct.
