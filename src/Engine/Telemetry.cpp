#include "Telemetry.h"
#include <iostream>
#include <print>

using namespace rosy;

///// Log

void log::debug(const std::string_view log_message) const
{
	if (level != log_level::debug) return;
	std::cout << log_message; // Intentionally do not use line endings automatically for debugging.
}

void log::info(const std::string_view log_message) const
{
	if (level > log_level::info) return;
	std::cout << log_message << '\n';
}

void log::warn(const std::string_view log_message) const
{
	if (level > log_level::warn) return;
	std::cout << log_message << '\n';
}

void log::error(const std::string_view log_message) const
{
	if (level == log_level::disabled) return;
	std::cerr << log_message << '\n';
}
