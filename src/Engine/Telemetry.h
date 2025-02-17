#pragma once
#include <string_view>

namespace rosy
{

    enum class log_level : uint8_t
    {
        debug,
        info,
        warn,
        error,
        disabled,
    };

    struct log
    {
        log_level level{log_level::info};
        void debug(std::string_view log_message) const;
        void info(std::string_view log_message) const;
        void warn(std::string_view log_message) const;
        void error(std::string_view log_message) const;
    };
}
