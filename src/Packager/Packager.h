#pragma once
#include "Asset/Asset.h"

namespace rosy_packager
{
    void optimize_mesh(const std::shared_ptr<rosy_logger::log>& l, rosy_asset::mesh& asset_mesh);
    [[nodiscard]] rosy::result generate_tangents(const std::shared_ptr<rosy_logger::log>& l, rosy_asset::asset& asset);
    [[nodiscard]] rosy::result generate_srgb_texture(const std::shared_ptr<rosy_logger::log>& l, const std::filesystem::path& image_path);
    [[nodiscard]] rosy::result generate_normal_map_texture(const std::shared_ptr<rosy_logger::log>& l, const std::filesystem::path& image_path);
}
