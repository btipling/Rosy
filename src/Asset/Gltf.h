#pragma once
#include "Asset.h"
#include "Logger/Logger.h"
#include <string>

namespace rosy_packager
{
    struct gltf_config
    {
        bool condition_images{true};
        bool use_mikktspace{true};
    };

    struct gltf
    {
        std::string source_path{};
        asset gltf_asset{};

        rosy::result import(std::shared_ptr<rosy_logger::log> l, gltf_config& cfg);
    };
}
