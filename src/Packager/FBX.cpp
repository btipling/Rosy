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
        for (uint64_t i{0}; i < 16; i++) a[i] = pos_r[i];
        return a;
    }

    FbxString get_attribute_type_name(const FbxNodeAttribute::EType type)
    {
        switch (type)
        {
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
        case FbxNodeAttribute::eLODGroup: return "lod_group";
        case FbxNodeAttribute::eSubDiv: return "sub_div";
        case FbxNodeAttribute::eCachedEffect:
        case FbxNodeAttribute::eLine:
            break;
        }
        return "unknown";
    }

    void print_attribute(const rosy::log* l, const FbxNodeAttribute* p_attribute)
    {
        if (!p_attribute) return;

        FbxString type_name = get_attribute_type_name(p_attribute->GetAttributeType());
        FbxString attr_name = p_attribute->GetName();
        // Note: to retrieve the character array of a FbxString, use its Buffer() method.
        l->info(std::format("<attribute type='{}' name='{}'/>\n", type_name.Buffer(), attr_name.Buffer()));
    }

    rosy::result traverse_node(const rosy::log* l, fbx_config& cfg, FbxNode* p_node, asset& fbx_asset)
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
        for (int i = 0; i < p_node->GetNodeAttributeCount(); i++)
        {
            const FbxNodeAttribute* attr = p_node->GetNodeAttributeByIndex(i);
            print_attribute(l, attr);
            if (attr->GetAttributeType() == FbxNodeAttribute::EType::eMesh)
            {
                mesh new_mesh{};
                const FbxMesh* l_mesh = p_node->GetMesh();
                l->info(std::format("we got ourselves a mesh folks, is all triangles IT BETTER BE: {}", l_mesh->IsTriangleMesh()));
                const auto pg_count = l_mesh->GetPolygonCount();
                l->info(std::format("polygon count: {}", pg_count));
                const FbxGeometryElementMaterial* l_mat = l_mesh->GetElementMaterial();
                if (l_mat == nullptr)
                {
                    l->info("no material");
                    continue;
                }

                const FbxLayerElementArrayTemplate<int>& l_mat_indices = l_mat->GetIndexArray();
                l->info(std::format("l_mat_indices count: {}", l_mat_indices.GetCount()));
                const FbxLayerElement::EMappingMode l_mat_mapping_mode = l_mat->GetMappingMode();
                switch (l_mat_mapping_mode)
                {
                case FbxLayerElement::EMappingMode::eByControlPoint:
                    l->info("eByControlPoint mesh mapping mode");
                    break;
                case FbxLayerElement::eNone:
                    l->info("eNone mesh mapping mode");
                    break;
                case FbxLayerElement::eByPolygonVertex:
                    l->info("eByPolygonVertex mesh mapping mode");
                    break;
                case FbxLayerElement::eByPolygon:
                    l->info("eByPolygon mesh mapping mode");
                    break;
                case FbxLayerElement::eByEdge:
                    l->info("eByEdge mesh mapping mode");
                    break;
                case FbxLayerElement::eAllSame:
                    l->info("eAllSame point mesh mapping mode");
                    break;
                }

                const auto num_normals = l_mesh->GetElementNormalCount();
                const auto num_uvs = l_mesh->GetElementUVCount();
                l->info(std::format("l_mesh num normals: {} num uvs: {}", num_normals, num_uvs));
                bool l_by_control_point{true};
                for (int ni{0}; ni < num_normals; ni++)
                {
                    const FbxGeometryElementNormal* n = l_mesh->GetElementNormal(ni);
                    l->info(std::format("normal mapping mode: {}", static_cast<uint8_t>(n->GetMappingMode())));
                    if (n->GetMappingMode() != FbxLayerElement::EMappingMode::eByControlPoint)
                    {
                        l_by_control_point = false;
                    }
                }
                for (int ni{0}; ni < num_uvs; ni++)
                {
                    const FbxGeometryElementUV* uv = l_mesh->GetElementUV(ni);
                    l->info(std::format("uv mapping mode: {}", static_cast<uint8_t>(uv->GetMappingMode())));
                    if (uv->GetMappingMode() != FbxLayerElement::EMappingMode::eByControlPoint)
                    {
                        l_by_control_point = false;
                    }
                }
                l->info(std::format("mapping mode by control point? {}", l_by_control_point));

                const FbxVector4* l_control_points = l_mesh->GetControlPoints();
                FbxVector4 l_current_vertex;
                FbxVector4 l_current_normal;
                FbxVector4 l_current_color;
                FbxVector2 l_current_uv;
                const char* l_uv_name{nullptr};
                int l_vertex_count{0};
                // Traversing the triangles

                // TODO figure out indices vs vertices, I think this below should just capture the indexes list and
                // for actually saving vertices to the mesh I need to use l_mesh->GetControlPointsCount(), starting at 0 and until > control point counts
                // and index into l_mesh->GetControlPoints()[i]; that gives me unrepeated vertices, which is not what's happening below

                for (int l_polygon_index{ 0 }; l_polygon_index < pg_count; l_polygon_index++)
                {
                    int l_mat_index = l_mat_indices.GetAt(l_polygon_index);
                    l->info(std::format("l_mat_index {}", l_mat_index));
                    int l_index_offset{ 0 };
                    int l_index_count{ 0 };
                    constexpr int num_vertices_in_triangle{ 3 };
                    for (int l_vertex_index = 0; l_vertex_index < num_vertices_in_triangle; l_vertex_index++) {

                        const int l_control_point_index = l_mesh->GetPolygonVertex(l_polygon_index, l_vertex_index);
                        l_current_vertex = l_control_points[l_control_point_index];

                        {
                            float x = static_cast<float>(l_current_vertex[0]);
                            float y = static_cast<float>(l_current_vertex[1]);
                            float z = static_cast<float>(l_current_vertex[2]);
                            l->info(std::format("position: ({}, {}, {})", x, y, z));
                        }
                        {
                            l_mesh->GetPolygonVertexNormal(l_polygon_index, l_vertex_index, l_current_normal);
                            float x = static_cast<float>(l_current_normal[0]);
                            float y = static_cast<float>(l_current_normal[1]);
                            float z = static_cast<float>(l_current_normal[2]);
                            l->info(std::format("normal: ({}, {}, {})", x, y, z));
                        }

                        {
                            bool l_unmapped_uv;
                            l_mesh->GetPolygonVertexUV(l_polygon_index, l_vertex_index, l_uv_name, l_current_uv, l_unmapped_uv);
                            float s = static_cast<float>(l_current_normal[0]);
                            float t = static_cast<float>(l_current_normal[1]);
                            l->info(std::format("uv: ({}, {}) mapped: {}", s, t, l_unmapped_uv));
                            if (l_uv_name != nullptr)
                            {
                                l->info(std::format("l_uv_name: {}", l_uv_name));
                            } else
                            {
                                l->info("no uv name");
                            }
                        }
                        if (l_mesh->GetElementVertexColorCount())
                        {
                            for (int lc{0}; lc < l_mesh->GetElementVertexColorCount(); lc++)
                            {
                                FbxColor vertex_color{};
                                switch (const FbxGeometryElementVertexColor* l_color = l_mesh->GetElementVertexColor(lc); l_color->GetMappingMode())
                                {
                                case FbxGeometryElement::eByControlPoint:
                                    l->info("color by control point");
                                    switch (l_color->GetReferenceMode())
                                    {
                                    case FbxGeometryElement::eDirect:
                                        vertex_color = l_color->GetDirectArray().GetAt(l_control_point_index);
                                        l->info(std::format("vertex color: ({}, {}, {}, {})", vertex_color.mRed, vertex_color.mGreen, vertex_color.mBlue, vertex_color.mAlpha));
                                        break;
                                    case FbxGeometryElement::eIndexToDirect:
                                    {
                                        const int id = l_color->GetIndexArray().GetAt(l_control_point_index);
                                        vertex_color = l_color->GetDirectArray().GetAt(id);
                                        l->info(std::format("vertex color: ({}, {}, {}, {})", vertex_color.mRed, vertex_color.mGreen, vertex_color.mBlue, vertex_color.mAlpha));
                                    }
                                    break;
                                    case FbxLayerElement::eIndex:
                                        break;
                                    }
                                    break;
                                case FbxGeometryElement::eByPolygonVertex:
                                    l->info("color by polygon vertex");
                                    switch (l_color->GetReferenceMode())
                                    {
                                    case FbxGeometryElement::eDirect:
                                        vertex_color = l_color->GetDirectArray().GetAt(l_control_point_index);
                                        l->info(std::format("vertex color: ({}, {}, {}, {})", vertex_color.mRed, vertex_color.mGreen, vertex_color.mBlue, vertex_color.mAlpha));
                                        break;
                                    case FbxGeometryElement::eIndexToDirect:
                                    {
                                        const int id = l_color->GetIndexArray().GetAt(l_control_point_index);
                                        vertex_color = l_color->GetDirectArray().GetAt(id);
                                        l->info(std::format("vertex color: ({}, {}, {}, {})", vertex_color.mRed, vertex_color.mGreen, vertex_color.mBlue, vertex_color.mAlpha));
                                    }
                                    break;
                                    case FbxLayerElement::eIndex:
                                        break;
                                    }
                                    break;
                                case FbxLayerElement::eNone:
                                case FbxLayerElement::eByPolygon:
                                case FbxLayerElement::eByEdge:
                                case FbxLayerElement::eAllSame:
                                    break;
                                }
                            }
                        } else
                        {
                            l->info("no vertex colors");
                        }


                        l_vertex_count += 1;
                    }
                    l->info(std::format("l_index_offset {} l_index_count {}", l_index_offset, l_index_count));
                }
                l->info(std::format("l_vertex_count {} ", l_vertex_count));
            }
        }

        // Recursively print the children.
        for (int j = 0; j < p_node->GetChildCount(); j++)
        {
            traverse_node(l, cfg, p_node->GetChild(j), fbx_asset);
        }

        l->info("</node>\n");
        return rosy::result::ok;
    }
}


rosy::result fbx::import(const rosy::log* l, [[maybe_unused]] fbx_config& cfg)
{
    const std::filesystem::path file_path{source_path};
    {
        constexpr glm::mat4 m{1.f};
        fbx_asset.asset_coordinate_system = mat4_to_array(m);
    }
    l->info("importing fbx starting");

    FbxManager* l_sdk_manager = FbxManager::Create();

    FbxIOSettings* ios = FbxIOSettings::Create(l_sdk_manager, IOSROOT);
    l_sdk_manager->SetIOSettings(ios);

    FbxImporter* l_importer = FbxImporter::Create(l_sdk_manager, "");

    if (const bool l_import_status = l_importer->Initialize(file_path.string().c_str(), -1, l_sdk_manager->GetIOSettings()); !l_import_status)
    {
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

    if (FbxNode* l_root_node = l_scene->GetRootNode())
    {
        for (int i = 0; i < l_root_node->GetChildCount(); i++)
            traverse_node(l, cfg, l_root_node->GetChild(i), fbx_asset);
    }
    else
    {
        l->info("no fbx root node?");
    }

    l_sdk_manager->Destroy();
    l->info("all done importing fbx asset");
    return rosy::result::ok;
}
