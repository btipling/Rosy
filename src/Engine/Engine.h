#pragma once
#pragma warning(4)
#pragma warning(error)
#include <cstdint>
#include <string_view>


// ReSharper disable once CppInconsistentNaming
typedef struct SDL_Window SDL_Window;

namespace rosy
{

	enum class result: uint8_t
	{
		ok,
		error,
	};

	enum class log_level: uint8_t
	{
		debug,
		info,
		warn,
		error,
		disabled,
	};

	struct log
	{
		log_level level{ log_level::debug };
		void debug(std::string_view log_message) const;
		void info(std::string_view log_message) const;
		void warn(std::string_view log_message) const;
		void error(std::string_view log_message) const;
	};

	struct engine
	{
		SDL_Window* window{ nullptr };
		log* l{ nullptr };

		result init();
		result run();
		void deinit();
	};
}