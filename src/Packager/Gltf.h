#pragma once
#include "Asset.h"
#include "../Engine/Telemetry.h"
#include <string>

namespace rosy_packager
{
    struct gltf
    {
        std::string source_path{};
        asset gltf_asset{};

        rosy::result import(rosy::log* l);
    };
}
