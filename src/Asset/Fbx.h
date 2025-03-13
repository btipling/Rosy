#pragma once
#include "Asset.h"
#include "Logger/Logger.h"
#include <string>

namespace rosy_packager
{
    struct fbx_config
    {
        bool condition_images{ true };
        bool use_mikktspace{ true };
    };

    struct fbx
    {
        std::string source_path{};
        asset fbx_asset{};

        rosy::result import(const std::shared_ptr<rosy_logger::log> l, fbx_config& cfg);
    };
}
