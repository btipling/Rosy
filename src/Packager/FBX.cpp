#include "pch.h"
#include "FBX.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.inl>

using namespace rosy_packager;


namespace
{
    std::array<float, 16> mat4_to_array(glm::mat4 m)
    {
        std::array<float, 16> a{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        const auto pos_r = glm::value_ptr(m);
        for (uint64_t i{ 0 }; i < 16; i++) a[i] = pos_r[i];
        return a;
    }
}

rosy::result fbx::import(const rosy::log* l, [[maybe_unused]] fbx_config& cfg)
{
    const std::filesystem::path file_path{ source_path };
    {
        constexpr glm::mat4 m{ 1.f };
        fbx_asset.asset_coordinate_system = mat4_to_array(m);
    }
    l->info("importing fbx starting");
    return rosy::result::ok;
}
