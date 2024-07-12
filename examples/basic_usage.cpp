#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LogLevel::INFO);
    logger.info("This message should appear in console");

    return 0;
}
