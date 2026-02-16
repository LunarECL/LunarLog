// culture_formatting.cpp
//
// Demonstrates locale-aware formatting in LunarLog.
//
// - setLocale() sets the logger-level locale for culture-specific specs
// - setSinkLocale() overrides the locale for individual sinks
// - Format specs: n/N (locale-aware number), d/D/t/T/f/F (date/time)
//
// Note: Exact output depends on available system locales and timezone.
// If a locale is not installed, LunarLog falls back to "C" silently.
//
// Compile: g++ -std=c++17 -I include examples/culture_formatting.cpp -o culture_formatting -pthread

#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE, false);  // no default console sink

    // Two file sinks: one for English, one for German formatting
    logger.addSink<minta::FileSink>("culture_en.log");   // sink 0
    logger.addSink<minta::FileSink>("culture_de.log");   // sink 1

    // ---------------------------------------------------------------
    // Logger-level locale
    //
    // Sets the default locale for all sinks. Affects :n, :N, :d, :D,
    // :t, :T, :f, :F format specifiers.
    // ---------------------------------------------------------------
    logger.setLocale("en_US");

    // Locale-aware number formatting: n / N
    // Adds thousand separators and locale-specific decimal point
    logger.info("Number: {amount:n}", 1234567.89);
    // en_US output: 1,234,567.89

    logger.info("Integer: {count:n}", 1000000);
    // en_US output: 1,000,000

    logger.info("Small: {val:n}", 0.5);
    // en_US output: 0.5

    // ---------------------------------------------------------------
    // Per-sink locale
    //
    // Override the locale for sink 1 to German. The same log message
    // gets rendered with different locales for each sink.
    // ---------------------------------------------------------------
    logger.setSinkLocale(1, "de_DE");

    logger.info("Total: {val:n}", 1234567.89);
    // sink 0 (en_US): Total: 1,234,567.89
    // sink 1 (de_DE): Total: 1.234.567,89

    // ---------------------------------------------------------------
    // Date/time formatting from Unix timestamps
    //
    // Format specs d/D/t/T/f/F interpret numeric values as Unix
    // timestamps (seconds since epoch). Non-numeric values pass through.
    //
    // Using timestamp 1705312245 = approximately Jan 15, 2024 10:30:45 UTC
    // Exact output varies by timezone.
    // ---------------------------------------------------------------
    long long ts = 1705312245;

    // Short date: d (format: %x, locale-dependent)
    logger.info("Short date: {ts:d}", ts);
    // en_US: e.g. 01/15/2024

    // Long date: D (format: weekday, month day, year)
    logger.info("Long date: {ts:D}", ts);
    // en_US: e.g. Monday, January 15, 2024

    // Short time: t (format: HH:MM)
    logger.info("Short time: {ts:t}", ts);
    // e.g. 10:30

    // Long time: T (format: HH:MM:SS)
    logger.info("Long time: {ts:T}", ts);
    // e.g. 10:30:45

    // Full date + short time: f
    logger.info("Full short: {ts:f}", ts);
    // e.g. Monday, January 15, 2024 10:30

    // Full date + long time: F
    logger.info("Full long: {ts:F}", ts);
    // e.g. Monday, January 15, 2024 10:30:45

    // ---------------------------------------------------------------
    // Combining number and date/time in one message
    // ---------------------------------------------------------------
    logger.info("Report: {count:n} entries processed on {date:D}", 50000, ts);
    // en_US: Report: 50,000 entries processed on Monday, January 15, 2024
    // de_DE: Report: 50.000 entries processed on Montag, Januar 15, 2024

    // ---------------------------------------------------------------
    // Changing locale at runtime
    //
    // setLocale() and setSinkLocale() are thread-safe and can be
    // called while other threads are actively logging.
    // ---------------------------------------------------------------
    logger.setLocale("fr_FR");
    logger.info("French number: {val:n}", 9876543.21);
    // fr_FR: e.g. 9 876 543,21 (space as thousand separator)

    // ---------------------------------------------------------------
    // Non-numeric values pass through date/time specs unchanged
    // ---------------------------------------------------------------
    logger.info("Not a timestamp: {val:D}", "hello");
    // Output: Not a timestamp: hello

    // ---------------------------------------------------------------
    // Default locale "C" — no culture-specific formatting
    // ---------------------------------------------------------------
    logger.setLocale("C");
    logger.info("C locale number: {val:n}", 1234567.89);
    // C locale: 1234567.89 (no thousand separators)

    // Flush to ensure all output is written
    logger.flush();

    std::cout << "Culture formatting examples complete."
              << "\nCheck culture_en.log (en_US / fr_FR formatting)"
              << "\nCheck culture_de.log (de_DE formatting)" << std::endl;
    return 0;
}
