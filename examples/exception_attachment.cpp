#include "lunar_log.hpp"
#include <iostream>
#include <stdexcept>

void riskyDatabaseQuery() {
    throw std::runtime_error("connection refused: host=db-01 port=5432");
}

void serviceLayer() {
    try {
        riskyDatabaseQuery();
    } catch (...) {
        std::throw_with_nested(std::logic_error("DB layer failed"));
    }
}

int main() {
    minta::LunarLog logger(minta::LogLevel::TRACE);

    logger.addSink<minta::FileSink>("exception.log");
    logger.addSink<minta::FileSink, minta::JsonFormatter>("exception.json.log");
    logger.addSink<minta::FileSink, minta::CompactJsonFormatter>("exception.jsonl.log");
    logger.addSink<minta::FileSink, minta::XmlFormatter>("exception.xml.log");

    std::cout << "=== Basic exception attachment ===" << std::endl;
    try {
        riskyDatabaseQuery();
    } catch (const std::exception& ex) {
        logger.error(ex, "Operation failed for user {name}", "john");
    }

    std::cout << "=== Exception-only (no message template) ===" << std::endl;
    try {
        riskyDatabaseQuery();
    } catch (const std::exception& ex) {
        logger.error(ex);
    }

    std::cout << "=== Nested exceptions ===" << std::endl;
    try {
        serviceLayer();
    } catch (const std::exception& ex) {
        logger.error(ex, "Request failed for endpoint {path}", "/api/users");
    }

    std::cout << "=== Exception on different log levels ===" << std::endl;
    try {
        throw std::invalid_argument("bad input: negative age");
    } catch (const std::exception& ex) {
        logger.warn(ex, "Validation failed");
    }

    try {
        throw std::out_of_range("index 42 exceeds size 10");
    } catch (const std::exception& ex) {
        logger.debug(ex, "Bounds check failed");
    }

    logger.flush();

    std::cout << "Check exception.log, exception.json.log, exception.jsonl.log, "
              << "and exception.xml.log for output." << std::endl;

    return 0;
}
