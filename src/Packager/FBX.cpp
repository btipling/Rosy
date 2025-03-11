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
    }    /**
     * Return a string-based representation based on the attribute type.
     */

    FbxString GetAttributeTypeName(FbxNodeAttribute::EType type) {
        switch (type) {
        case FbxNodeAttribute::eUnknown: return "unidentified";
        case FbxNodeAttribute::eNull: return "null";
        case FbxNodeAttribute::eMarker: return "marker";
        case FbxNodeAttribute::eSkeleton: return "skeleton";
        case FbxNodeAttribute::eMesh: return "mesh";
        case FbxNodeAttribute::eNurbs: return "nurbs";
        case FbxNodeAttribute::ePatch: return "patch";
        case FbxNodeAttribute::eCamera: return "camera";
        case FbxNodeAttribute::eCameraStereo: return "stereo";
        case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
        case FbxNodeAttribute::eLight: return "light";
        case FbxNodeAttribute::eOpticalReference: return "optical reference";
        case FbxNodeAttribute::eOpticalMarker: return "marker";
        case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
        case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
        case FbxNodeAttribute::eBoundary: return "boundary";
        case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
        case FbxNodeAttribute::eShape: return "shape";
        case FbxNodeAttribute::eLODGroup: return "lodgroup";
        case FbxNodeAttribute::eSubDiv: return "subdiv";
        default: return "unknown";
        }
    }

    void print_attribute(const rosy::log* l, const FbxNodeAttribute* p_attribute) {
        if (!p_attribute) return;

        FbxString type_name = GetAttributeTypeName(p_attribute->GetAttributeType());
        FbxString attr_name = p_attribute->GetName();
        // Note: to retrieve the character array of a FbxString, use its Buffer() method.
        l->info(std::format("<attribute type='{}' name='{}'/>\n", type_name.Buffer(), attr_name.Buffer()));
    }

    rosy::result print_node(const rosy::log* l, fbx_config& cfg, FbxNode* p_node)
    {
        const char* node_name = p_node->GetName();
        FbxDouble3 translation = p_node->LclTranslation.Get();
        FbxDouble3 rotation = p_node->LclRotation.Get();
        FbxDouble3 scaling = p_node->LclScaling.Get();

        // Print the contents of the node.
        l->info(std::format("<node name='{}' translation='({}, {}, {})' rotation='({}, {}, {})' scaling='({}, {}, {})'>",
            node_name,
            translation[0], translation[1], translation[2],
            rotation[0], rotation[1], rotation[2],
            scaling[0], scaling[1], scaling[2]
        ));

        // Print the node's attributes.
        for (int i = 0; i < p_node->GetNodeAttributeCount(); i++) {
            const FbxNodeAttribute* attr = p_node->GetNodeAttributeByIndex(i);
            print_attribute(l, attr);
            if (attr->GetAttributeType() == FbxNodeAttribute::EType::eMesh)
            {
                const FbxMesh* l_mesh = p_node->GetMesh();
                l->info(std::format("we got ourselves a mesh folks, is all triangles IT BETTER BE: {}", l_mesh->IsTriangleMesh()));
            }

        }

        // Recursively print the children.
        for (int j = 0; j < p_node->GetChildCount(); j++)
            print_node(l, cfg, p_node->GetChild(j));

        l->info("</node>\n");
        return rosy::result::ok;
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

    if (FbxNode* l_root_node = l_scene->GetRootNode()) {
        for (int i = 0; i < l_root_node->GetChildCount(); i++)
            print_node(l, cfg, l_root_node->GetChild(i));
    } else
    {
        l->info("no fbx root node?");
    }

    l_sdk_manager->Destroy();
    l->info("all done importing fbx asset");
    return rosy::result::ok;
}
