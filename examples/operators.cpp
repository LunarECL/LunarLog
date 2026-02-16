// operators.cpp
//
// Demonstrates the @ (destructure) and $ (stringify) operators from the
// MessageTemplates specification.
//
// - @ hints structured formatters to preserve native types (numbers, booleans)
// - $ forces the value to always be captured as a string
// - In human-readable output, both operators are transparent
//
// Compile: g++ -std=c++17 -I include examples/operators.cpp -o operators -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    // Create logger — default ConsoleSink shows human-readable output
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // Add JSON and XML file sinks to see how operators affect structured output
    logger.addSink<minta::FileSink, minta::JsonFormatter>("operators.json.log");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("operators.xml.log");

    // ---------------------------------------------------------------
    // Basic operator usage
    // ---------------------------------------------------------------

    // @ (destructure): In JSON, numeric/boolean values become native types
    logger.info("User: {@id}, Active: {@active}", 42, true);
    // JSON: {"properties":{"id":42,"active":true}}

    // $ (stringify): Forces values to be emitted as strings in JSON
    logger.info("User: {$id}, Active: {$active}", 42, true);
    // JSON: {"properties":{"id":"42","active":"true"}}

    // No operator: values are emitted as strings by default
    logger.info("User: {id}, Active: {active}", 42, true);
    // JSON: {"properties":{"id":"42","active":"true"}}

    // ---------------------------------------------------------------
    // Operators with different types
    // ---------------------------------------------------------------

    // Integers
    logger.info("Count: {@count}", 100);               // JSON: 100
    logger.info("Count: {$count}", 100);               // JSON: "100"

    // Floating-point
    logger.info("Price: {@price}", 19.99);             // JSON: 19.99
    logger.info("Price: {$price}", 19.99);             // JSON: "19.99"

    // Booleans
    logger.info("Enabled: {@flag}", true);             // JSON: true
    logger.info("Enabled: {$flag}", true);             // JSON: "true"

    // Strings — both operators produce the same JSON string output
    logger.info("Name: {@name}", "alice");             // JSON: "alice"
    logger.info("Name: {$name}", "alice");             // JSON: "alice"

    // ---------------------------------------------------------------
    // Operators combined with format specifiers
    //
    // The format spec controls the rendered message text. The operator
    // controls how the RAW value (pre-formatting) is stored in properties.
    // ---------------------------------------------------------------
    logger.info("Amount: {@amount:.2f}", 3.14159);
    // Message: "Amount: 3.14"
    // JSON property: "amount": 3.14159 (raw value, not formatted)

    logger.info("Hex: {@val:X}", 255);
    // Message: "Hex: FF"
    // JSON property: "val": 255

    logger.info("Pct: {$rate:P}", 0.856);
    // Message: "Pct: 85.60%"
    // JSON property: "rate": "0.856" (string because of $)

    // ---------------------------------------------------------------
    // Invalid operator forms — treated as literal text
    // ---------------------------------------------------------------
    logger.info("Invalid: {@}");                        // literal {@}
    logger.info("Invalid: {@@x}", 1);                   // literal {@@x}
    logger.info("Invalid: {@ x}", 1);                   // literal {@ x}

    // ---------------------------------------------------------------
    // XML output — operators add attributes to property elements
    //
    // @ adds destructure="true"
    // $ adds stringify="true"
    // ---------------------------------------------------------------
    logger.info("XML demo: {@num} and {$label}", 42, "test");
    // XML: <num destructure="true">42</num>
    //      <label stringify="true">test</label>

    // Flush to ensure all async output is written
    logger.flush();

    std::cout << "\nHuman-readable output shown above."
              << "\nCheck operators.json.log for JSON structured output."
              << "\nCheck operators.xml.log for XML structured output." << std::endl;
    return 0;
}
