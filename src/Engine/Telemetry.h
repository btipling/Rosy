#pragma once
#include "Types.h"
#include <string_view>

namespace rosy
{

	struct log
	{
		log_level level{ log_level::info };
		void debug(std::string_view log_message) const;
		void info(std::string_view log_message) const;
		void warn(std::string_view log_message) const;
		void error(std::string_view log_message) const;
	};
}