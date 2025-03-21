#pragma once
#include <string_view>

namespace rosy_logger
{

    enum class log_level : uint8_t
    {
        debug,
        info,
        warn,
        error,
        disabled,
    };

    class log
    {
    public:
        log_level level{log_level::info};
        void debug(std::string_view log_message) const;
        void info(std::string_view log_message) const;
        void warn(std::string_view log_message) const;
        void error(std::string_view log_message) const;
    };
}
