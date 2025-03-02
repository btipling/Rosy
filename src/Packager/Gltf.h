#pragma once
#include "Asset.h"
#include "../Engine/Telemetry.h"
#include <string>

namespace rosy_packager
{
    struct gltf_config
    {
        bool condition_images{ true };
    };

    struct gltf
    {
        std::string source_path{};
        asset gltf_asset{};

        rosy::result import(rosy::log* l, gltf_config& cfg);
    };
}
