// format_specifiers.cpp
//
// Demonstrates every format specifier supported by LunarLog.
//
// Numeric specs: .Nf (fixed-point), X/x (hex), C (currency), P (percentage),
//                E/e (scientific), 0N (zero-padded)
// Culture specs: n/N (locale-aware number), d/D (date), t/T (time), f/F (full)
//
// Compile: g++ -std=c++17 -I include examples/format_specifiers.cpp -o format_specifiers -pthread

#include "lunar_log.hpp"
#include <iostream>
#include <ctime>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    // ---------------------------------------------------------------
    // Fixed-point: .Nf — control decimal places
    // ---------------------------------------------------------------
    logger.info("Fixed .2f:  {val:.2f}", 3.14159);    // 3.14
    logger.info("Fixed .4f:  {val:.4f}", 3.14159);    // 3.1416
    logger.info("Fixed .0f:  {val:.0f}", 3.14159);    // 3
    logger.info("Shorthand 2f: {val:2f}", 3.14159);   // 3.14 (same as .2f)

    // ---------------------------------------------------------------
    // Hex: X (uppercase) and x (lowercase)
    // ---------------------------------------------------------------
    logger.info("Hex upper: {val:X}", 255);            // FF
    logger.info("Hex lower: {val:x}", 255);            // ff
    logger.info("Hex large: {val:X}", 65535);          // FFFF
    logger.info("Hex negative: {val:X}", -1);          // -1

    // ---------------------------------------------------------------
    // Currency: C — prepends $ and formats to 2 decimals
    // ---------------------------------------------------------------
    logger.info("Currency: {price:C}", 9.99);          // $9.99
    logger.info("Currency neg: {price:C}", -42.5);     // -$42.50
    logger.info("Currency zero: {price:C}", 0.0);      // $0.00
    logger.info("Currency large: {price:C}", 1234.5);  // $1234.50

    // ---------------------------------------------------------------
    // Percentage: P — multiplies by 100 and appends %
    // ---------------------------------------------------------------
    logger.info("Percent: {rate:P}", 0.856);           // 85.60%
    logger.info("Percent full: {rate:P}", 1.0);        // 100.00%
    logger.info("Percent small: {rate:P}", 0.005);     // 0.50%

    // ---------------------------------------------------------------
    // Scientific: E (uppercase) and e (lowercase)
    // ---------------------------------------------------------------
    logger.info("Sci upper: {val:E}", 1500.0);         // 1.500000E+03
    logger.info("Sci lower: {val:e}", 1500.0);         // 1.500000e+03
    logger.info("Sci small: {val:e}", 0.00042);        // 4.200000e-04

    // ---------------------------------------------------------------
    // Zero-padded: 0N — pad integer to N digits
    // ---------------------------------------------------------------
    logger.info("Padded 04: {id:04}", 42);             // 0042
    logger.info("Padded 08: {id:08}", 42);             // 00000042
    logger.info("Padded 02: {id:02}", 7);              // 07
    logger.info("Padded neg: {id:04}", -5);            // -0005

    // ---------------------------------------------------------------
    // Namespaced placeholders — spec splits on the last colon
    // ---------------------------------------------------------------
    logger.info("Namespaced: {ns:key:.2f}", 3.14159);  // 3.14

    // ---------------------------------------------------------------
    // Combining operators with format specs
    // ---------------------------------------------------------------
    // The @ operator is transparent in human-readable output but
    // affects structured formatters (JSON/XML). Format spec still applies.
    logger.info("Operator + spec: {@amount:.2f}", 3.14159);  // 3.14

    // ---------------------------------------------------------------
    // Culture-specific: n/N — locale-aware number formatting
    // Requires setLocale() to see locale-specific separators.
    // ---------------------------------------------------------------
    logger.setLocale("en_US");
    logger.info("Number (en_US): {amount:n}", 1234567.89);   // 1,234,567.89

    // ---------------------------------------------------------------
    // Culture-specific: date/time from Unix timestamps
    // Using a known timestamp: 1705312245 = Jan 15, 2024 ~10:30:45 UTC
    // Exact output depends on local timezone.
    // ---------------------------------------------------------------
    long long ts = 1705312245;

    logger.info("Short date (d):      {ts:d}", ts);    // e.g. 01/15/2024
    logger.info("Long date (D):       {ts:D}", ts);    // e.g. Monday, January 15, 2024
    logger.info("Short time (t):      {ts:t}", ts);    // e.g. 10:30
    logger.info("Long time (T):       {ts:T}", ts);    // e.g. 10:30:45
    logger.info("Full short (f):      {ts:f}", ts);    // e.g. Monday, January 15, 2024 10:30
    logger.info("Full long (F):       {ts:F}", ts);    // e.g. Monday, January 15, 2024 10:30:45

    // Non-numeric values pass through unchanged
    logger.info("Non-numeric date: {ts:d}", "not-a-timestamp");

    // ---------------------------------------------------------------
    // Unknown specs are returned as-is (no error)
    // ---------------------------------------------------------------
    logger.info("Unknown spec: {val:Z}", 42);           // 42

    // Flush to ensure all async output is written before exit
    logger.flush();

    std::cout << "\nAll format specifier examples logged above." << std::endl;
    return 0;
}
