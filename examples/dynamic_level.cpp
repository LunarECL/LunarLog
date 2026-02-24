#include "lunar_log.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

int main() {
    // ── 1. LevelSwitch — immediate runtime level changes ──

    std::cout << "=== LevelSwitch Demo ===" << std::endl;

    auto levelSwitch = std::make_shared<minta::LevelSwitch>(minta::LogLevel::INFO);

    auto logger = minta::LunarLog::configure()
        .minLevel(levelSwitch)
        .writeTo<minta::ConsoleSink>("console")
        .build();

    logger.info("Application started (level=INFO)");
    logger.debug("This debug message is hidden");

    // Change level at runtime — takes effect immediately
    levelSwitch->set(minta::LogLevel::DEBUG);
    logger.debug("Debug is now visible (level changed to DEBUG)");

    levelSwitch->set(minta::LogLevel::WARN);
    logger.info("Info is now hidden (level changed to WARN)");
    logger.warn("Warnings are still visible");
    logger.flush();

    // ── 2. Shared LevelSwitch between loggers ──

    std::cout << "\n=== Shared LevelSwitch ===" << std::endl;

    auto shared = std::make_shared<minta::LevelSwitch>(minta::LogLevel::ERROR);

    auto log1 = minta::LunarLog::configure()
        .minLevel(shared)
        .enrich(minta::Enrichers::property("source", "logger-1"))
        .writeTo<minta::ConsoleSink>("c1")
        .build();

    auto log2 = minta::LunarLog::configure()
        .minLevel(shared)
        .enrich(minta::Enrichers::property("source", "logger-2"))
        .writeTo<minta::ConsoleSink>("c2")
        .build();

    log1.warn("Logger-1: hidden at ERROR level");
    log2.warn("Logger-2: hidden at ERROR level");

    shared->set(minta::LogLevel::WARN);

    log1.warn("Logger-1: now visible!");
    log2.warn("Logger-2: now visible!");
    log1.flush();
    log2.flush();

    // ── 3. Config file watcher ──

    std::cout << "\n=== Config File Watcher ===" << std::endl;

    // Write initial config
    const std::string configPath = "lunarlog.json";
    {
        std::ofstream ofs(configPath.c_str());
        ofs << "{\n"
            << "    \"minLevel\": \"WARN\",\n"
            << "    \"filters\": []\n"
            << "}\n";
    }

    auto sw = std::make_shared<minta::LevelSwitch>(minta::LogLevel::WARN);

    auto watched = minta::LunarLog::configure()
        .minLevel(sw)
        .watchConfig(configPath, std::chrono::seconds(2))
        .writeTo<minta::ConsoleSink>("watched")
        .build();

    watched.warn("Initial: WARN level active");
    watched.info("Initial: INFO is hidden");
    watched.flush();

    std::cout << "\nUpdating config to DEBUG level..." << std::endl;

    // Update config to DEBUG
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::ofstream ofs(configPath.c_str());
        ofs << "{\n"
            << "    \"minLevel\": \"DEBUG\",\n"
            << "    \"filters\": []\n"
            << "}\n";
    }

    // Wait for the watcher to pick up the change
    std::this_thread::sleep_for(std::chrono::seconds(3));

    watched.debug("After reload: DEBUG is now visible!");
    watched.info("After reload: INFO is now visible!");
    watched.flush();

    // Clean up
    std::remove(configPath.c_str());

    std::cout << "\nDynamic level examples complete." << std::endl;
    return 0;
}
