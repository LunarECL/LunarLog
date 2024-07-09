#include "lunar_log.hpp"
#include <iostream>

int main() {
    minta::LunarLog logger(minta::LunarLog::Level::DEBUG, "log.txt", 1024 * 1024, true);
    logger.info("Starting the application");
    logger.debug("Debugging information: x = {}", 42);
    logger.warn("This is a warning with a named placeholder: {placeholder}", "example");
    logger.error("An error occurred: error code {}", -1);
    logger.fatal("Fatal error: system crash");

    return 0;
}
