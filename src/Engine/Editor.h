#pragma once
#include "Types.h"
#include "Telemetry.h"


namespace rosy
{
    struct editor
    {
        [[nodiscard]] result init(const std::shared_ptr<log>& new_log, [[maybe_unused]] config new_cfg);
        [[nodiscard]] result process(const level_editor_commands& commands, level_editor_state* state);
        void deinit();
    };
}
