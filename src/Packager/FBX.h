#pragma once
#include "Asset.h"
#include "../Engine/Telemetry.h"
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

        rosy::result import(const rosy::log* l, fbx_config& cfg);
    };
}
