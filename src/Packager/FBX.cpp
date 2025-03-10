#include "pch.h"
#include "FBX.h"
#include <fbxsdk.h>
#include <fbxsdk/fileio/fbxiosettings.h>
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

    FbxManager* l_sdk_manager = FbxManager::Create();

    FbxIOSettings* ios = FbxIOSettings::Create(l_sdk_manager, IOSROOT);
    l_sdk_manager->SetIOSettings(ios);

    FbxImporter* l_importer = FbxImporter::Create(l_sdk_manager, "");

    if (const bool l_import_status = l_importer->Initialize(file_path.string().c_str(), -1, l_sdk_manager->GetIOSettings()); !l_import_status) {
        l->error("Call to FbxImporter::Initialize() failed.");
        l->error(std::format("Error returned: {}", l_importer->GetStatus().GetErrorString()));
        return rosy::result::read_failed;
    }
    l->info("importing fbx success?");

    FbxScene* l_scene = FbxScene::Create(l_sdk_manager, "myScene");

    if (const bool l_import_status = l_importer->Import(l_scene); !l_import_status)
    {
        l->error("Call to FbxImporter::Import() failed.");
        l->error(std::format("Error returned: {}", l_importer->GetStatus().GetErrorString()));
        return rosy::result::read_failed;
    }
    l_importer->Destroy();
    l->info("importing fbx scene success?");

    return rosy::result::ok;
}
