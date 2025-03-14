#include "pch.h"
#include "Logger.h"

using namespace rosy_logger;

///// Log

void log::debug(const std::string_view log_message) const
{
    if (level != log_level::debug) return;
    std::cout << "[" << std::chrono::system_clock::now() << "] [DEBUG] " << log_message << '\n';
}

void log::info(const std::string_view log_message) const
{
    if (level > log_level::info) return;
    std::cout << "[" << std::chrono::system_clock::now() << "] [INFO] " << log_message << '\n';
}

void log::warn(const std::string_view log_message) const
{
    if (level > log_level::warn) return;
    std::cout << "[" << std::chrono::system_clock::now() << "] [WARN] " << log_message << '\n';
}

void log::error(const std::string_view log_message) const
{
    if (level == log_level::disabled) return;
    std::cerr << "[" << std::chrono::system_clock::now() << "] [ERROR] " << log_message << '\n';
}
