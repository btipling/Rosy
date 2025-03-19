#pragma once
#include "Types.h"
#include "Logger/Logger.h"


namespace rosy
{
    struct editor
    {
        [[nodiscard]] result init(const std::shared_ptr<rosy_logger::log>& new_log, [[maybe_unused]] config new_cfg);
        [[nodiscard]] result process(const read_level_state& rls, const level_editor_commands& commands, level_editor_state* state);
        void deinit();
    };
}
