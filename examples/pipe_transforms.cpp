// pipe_transforms.cpp
//
// Demonstrates pipe transforms — post-format value transformations applied
// with the | (pipe) operator inside placeholders.
//
// Key concepts:
//   - Pipe syntax: {name|transform}, {name|t1|t2} (chaining, left-to-right)
//   - Combining with format specifiers: {name:.2f|comma}
//   - Combining with @ and $ operators: {@name|upper}
//   - String transforms: upper, lower, trim, truncate, pad, padl, quote
//   - Number transforms: comma, hex, oct, bin, bytes, duration, pct
//   - Structural transforms: json, type, expand, str
//   - Unknown transforms pass through unchanged (fail-open)
//
// Compile: g++ -std=c++17 -I include examples/pipe_transforms.cpp -o pipe_transforms -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // ---------------------------------------------------------------
    // 1. String transforms
    // ---------------------------------------------------------------

    // upper / lower — ASCII case conversion
    logger.info("Upper: {name|upper}", "alice");           // ALICE
    logger.info("Lower: {name|lower}", "HELLO");           // hello

    // trim — strip leading/trailing whitespace
    logger.info("Trim: [{val|trim}]", "  hello  ");        // [hello]

    // truncate:N — limit to N UTF-8 codepoints, append … if truncated
    logger.info("Truncate: {msg|truncate:10}", "Hello, World! This is long");
    // Hello, Wor…

    // pad:N — right-pad with spaces to N codepoints
    logger.info("Padded: [{name|pad:10}]", "Alice");       // [Alice     ]

    // padl:N — left-pad with spaces to N codepoints
    logger.info("Left-padded: [{val|padl:8}]", "42");      // [      42]

    // quote — wrap in double quotes
    logger.info("Quoted: {path|quote}", "/usr/local/bin");  // "/usr/local/bin"

    // ---------------------------------------------------------------
    // 2. Number transforms
    // ---------------------------------------------------------------

    // comma — thousands separators
    logger.info("Comma: {amount|comma}", 1234567);          // 1,234,567
    logger.info("Comma float: {val|comma}", 1234567.89);    // 1,234,567.89

    // hex — hexadecimal with 0x prefix
    logger.info("Hex: {val|hex}", 255);                     // 0xff
    logger.info("Hex: {val|hex}", 4096);                    // 0x1000

    // oct — octal with 0 prefix
    logger.info("Octal: {val|oct}", 8);                     // 010
    logger.info("Octal: {val|oct}", 255);                   // 0377

    // bin — binary with 0b prefix
    logger.info("Binary: {val|bin}", 10);                   // 0b1010
    logger.info("Binary: {val|bin}", 255);                  // 0b11111111

    // bytes — human-readable byte sizes
    logger.info("Bytes: {size|bytes}", 0);                  // 0 B
    logger.info("Bytes: {size|bytes}", 1024);               // 1.0 KB
    logger.info("Bytes: {size|bytes}", 1048576);            // 1.0 MB
    logger.info("Bytes: {size|bytes}", 1073741824);         // 1.0 GB

    // duration — human-readable time from milliseconds
    logger.info("Duration: {ms|duration}", 500);            // 500ms
    logger.info("Duration: {ms|duration}", 61000);          // 1m 1s
    logger.info("Duration: {ms|duration}", 3661000);        // 1h 1m 1s

    // pct — percentage (multiplies by 100, appends %)
    logger.info("Percent: {rate|pct}", 0.856);              // 85.6%
    logger.info("Percent: {rate|pct}", 1.0);                // 100.0%

    // ---------------------------------------------------------------
    // 3. Structural transforms
    // ---------------------------------------------------------------

    // json — force JSON serialization of the value
    logger.info("JSON string: {val|json}", "hello world");  // "hello world"
    logger.info("JSON number: {val|json}", 42);             // 42
    logger.info("JSON bool: {val|json}", true);             // true

    // type — output the detected C++ type name
    logger.info("Type of int: {val|type}", 42);             // int
    logger.info("Type of float: {val|type}", 3.14);         // double
    logger.info("Type of str: {val|type}", "hello");        // string
    logger.info("Type of bool: {val|type}", true);          // bool

    // expand and str — these are operator aliases that affect structured
    // output (JSON/XML properties). In human-readable output, they are
    // transparent — the value appears unchanged.
    logger.info("Expand: {val|expand}", 42);                // 42
    logger.info("Stringify: {val|str}", 42);                // 42

    // ---------------------------------------------------------------
    // 4. Chaining — left-to-right, compose multiple transforms
    // ---------------------------------------------------------------

    // Trim then uppercase
    logger.info("Chained: {val|trim|upper}", "  hello  "); // HELLO

    // Format as fixed-point, then add comma separators
    logger.info("Chain number: {val:.2f|comma}", 1234567.89);
    // 1,234,567.89

    // Truncate, then quote
    logger.info("Chain string: {name|truncate:5|quote}", "Alexander");
    // "Alexan…" — wait, truncate:5 → "Alexa…", then quote → "\"Alexa…\""

    // Upper then pad for aligned columns
    logger.info("Aligned: [{level|upper|pad:8}] {msg}", "info", "Server started");
    // [INFO    ] Server started

    // ---------------------------------------------------------------
    // 5. Combining with format specifiers
    //
    // Format spec is applied FIRST, then pipe transforms.
    // Syntax: {name:spec|transform}
    // ---------------------------------------------------------------

    // Fixed-point format, then thousands separator
    logger.info("Spec + pipe: {revenue:.2f|comma}", 9876543.21);
    // 9,876,543.21

    // Zero-padded, then quoted (spec 04, then quote)
    logger.info("Spec + pipe: {id:04|quote}", 42);
    // "0042"

    // Currency format, then uppercase (just to show it works)
    logger.info("Spec + pipe: {price:C|upper}", 9.99);
    // $9.99 (no lowercase to convert, but demonstrates combining)

    // ---------------------------------------------------------------
    // 6. Combining with @ and $ operators
    //
    // Operators affect structured output; transforms affect the
    // rendered message. Both can be used together.
    // ---------------------------------------------------------------

    // Destructure operator + pipe transform
    logger.info("Op + pipe: {@amount:.2f|comma}", 1234567.89);
    // Message: 1,234,567.89
    // In JSON: {"amount": 1234567.89} (raw value, native type)

    // Stringify operator + pipe transform
    logger.info("Op + pipe: {$code|upper}", "abc123");
    // Message: ABC123
    // In JSON: {"code": "abc123"} (always string)

    // ---------------------------------------------------------------
    // 7. Error handling — unknown transforms pass through
    //
    // If a transform name is not recognized, the value is passed
    // through unchanged. This is a fail-open design.
    // ---------------------------------------------------------------
    logger.info("Unknown: {val|nonexistent}", "hello");     // hello
    logger.info("Mixed: {val|upper|nonexistent|quote}", "hello");
    // "HELLO" (upper applies, nonexistent skips, quote applies)

    // ---------------------------------------------------------------
    // 8. Non-numeric values with number transforms
    //
    // Number transforms return the value as-is if it can't be parsed.
    // ---------------------------------------------------------------
    logger.info("Comma on string: {val|comma}", "not-a-number");
    // not-a-number (returned unchanged)

    logger.info("Bytes on string: {val|bytes}", "hello");
    // hello (returned unchanged)

    // Flush to ensure all output is written
    logger.flush();

    std::cout << "\nPipe transform examples complete." << std::endl;
    return 0;
}
