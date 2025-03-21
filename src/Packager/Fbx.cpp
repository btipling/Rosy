#include "pch.h"
#include "FBX.h"
#include <fbxsdk.h>
#include <fbxsdk/fileio/fbxiosettings.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <meshoptimizer.h>

#include "Packager.h"

using namespace rosy_packager;

// FBX import is best used with flattened mesh nodes that may have optional animation. Unlike with the GLTF import, for FBX images are not extracted from the asset.
// This import looks for three images that can exist alongside the FBX file:
// - a color image that ends with "albedo.tga"
// - a normal map image that ends with "normal.tga"
// - a mixmap that ends with "mixmap.tga"
//   - the red channel in mixmap is ambient occlusion
//   - the green channel in mixmap is roughness
//   - the blue channel in mixmap is metalness
//   - the alpha channel in mixmap is unused
// The name of the image files must contain a substring matching the mesh's node in order to be successfully mapped to the node.
// Support for surfaces of a mesh having their own images is a TODO.

// A "Control Point" is an FBX term for what everyone else in the world calls a vertex or vertex attribute data, were it not for the fact that we aren't guaranteed to get all the vertices
// we need to render in a graphics API like Vulkan or OpenGL via these control points. The control points are the unique set of positions in a mesh, but non-trivial meshes reuse vertices with
// a different combination of normals and texture coordinates. A format like GLTF just gives you these, but FBX we need to create a vector of number of triangles * number of vertices in a triangle
// and then build our own index array from that will definitely duplicate vertices/normal/uv values and needs to be run through a mesh optimizer. Once we have the actual index list we need to
// build the actual index buffer which is basically a list of lists. Each material has a list of indices. The index buffer for each mesh is that list of those lists.
// There is a by control point mapped mode that does this the standard way also, I am just going to try and ignore that?
//
// So we assume we have:
// 1. A unique list of vertices
// 2. A list of triangles where for each of the three vertices of the triangle we can get a position, normal, uvs and a vertex color and the triangle's material index
// 3. A list of materials
//
// What needs to happen is we build a list of vertices, build a list of indices, and add the index matching a material into a material index list, the eventual material index list is the sum of those.
// Then we maintain a list of surfaces which map to their respective list within the overall index list with an offset and a count.
//
// Then we run it all through a mesh optimizer and map any removed/moved indices for the surfaces.
//
// for the purpose of animation we have to consider the same vertex with different weights/links to bones each as unique vertices as well and not remove any of those once we have them.
//
// A "Polygon" is a triangle. In FBX and Maya it can also be a quad but this is not supported by Rosy.
// There is a property called "unmapped" uvs (texture coordinates) and is a thing that is possible in FBX and Maya, I don't understand its implications in Rosy.
// as I seem to be able to get one for each vertex via the GetPolygonVertexUV call.
//
// A thing I haven't determined yet, but am maybe incorrectly assuming is that nodes do not share meshes. I saw some code about instancing somewhere so that may be incorrect.

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

    void print_attribute(const std::shared_ptr<rosy_logger::log>& l, const FbxNodeAttribute* p_attribute)
    {
        if (!p_attribute) return;

        FbxString type_name = get_attribute_type_name(p_attribute->GetAttributeType());
        FbxString attr_name = p_attribute->GetName();
        // Note: to retrieve the character array of a FbxString, use its Buffer() method.
        l->info(std::format("<attribute type='{}' name='{}'/>\n", type_name.Buffer(), attr_name.Buffer()));
    }

    rosy::result traverse_node(const std::shared_ptr<rosy_logger::log>& l, fbx_config& cfg, FbxNode* p_node, rosy_asset::asset& fbx_asset)
    {
        const auto node_name = std::string{ p_node->GetName() };
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
                rosy_asset::mesh new_asset_mesh{};
                const FbxMesh* fbx_mesh = p_node->GetMesh();
                if (!fbx_mesh->IsTriangleMesh())
                {
                    l->info("Not a triangle mesh.");
                }
                const auto triangle_count = fbx_mesh->GetPolygonCount();
                const auto vertices_count = fbx_mesh->GetControlPointsCount();
                l->info(std::format("triangle count: {}", triangle_count));
                const FbxGeometryElementMaterial* materials = fbx_mesh->GetElementMaterial();
                if (materials == nullptr)
                {
                    l->info("no material");
                    continue;
                }

                rosy_asset::material new_asset_mat{};
                uint32_t img_index{ 0 };
                for (const auto& img : fbx_asset.images)
                {
                    if (const auto n = std::string{ img.name.begin(), img.name.end() }; n.contains(node_name))
                    {
                        if (n.ends_with("normal.dds"))
                        {
                            new_asset_mat.normal_image_index = img_index;
                            img_index += 1;
                            continue;
                        }
                        if (n.ends_with("mixmap.dds"))
                        {
                            new_asset_mat.mixmap_image_index = img_index;
                            img_index += 1;
                            continue;
                        }
                        if (n.ends_with("albedo.dds"))
                        {
                            new_asset_mat.color_image_index = img_index;
                            img_index += 1;
                            continue;
                        }
                        if (new_asset_mat.normal_image_index < UINT32_MAX && new_asset_mat.mixmap_image_index < UINT32_MAX && new_asset_mat.color_image_index < UINT32_MAX) break;
                    }
                    img_index += 1;
                }

                // GLTF import doesn't support these:
                new_asset_mat.metallic_image_index = UINT32_MAX;
                const auto asset_material_index = static_cast<uint32_t>(fbx_asset.materials.size());
                fbx_asset.materials.push_back(new_asset_mat);

                // material_indices lets us look up the index of the material for a triangle, which lets us create surfaces for each material
                // triangles get assigned to surfaces, each surface has a unique material
                const FbxLayerElementArrayTemplate<int>& material_indices = materials->GetIndexArray();
                const int num_materials = material_indices.GetCount();

                std::vector<std::vector<uint32_t>> asset_materials_index_list; // The list of lists that builds the asset's index buffer for this mesh.
                asset_materials_index_list.resize(num_materials);

                l->info(std::format("rsy_mat_indices count: {}", num_materials));
                switch (materials->GetMappingMode())
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

                const auto num_normal_elements = fbx_mesh->GetElementNormalCount();
                const auto num_uv_elements = fbx_mesh->GetElementUVCount();
                l->info(std::format("rsy_mesh num normals: {} num uvs: {}", num_normal_elements, num_uv_elements));
                for (int ni{0}; ni < num_normal_elements; ni++)
                {
                    const FbxGeometryElementNormal* n = fbx_mesh->GetElementNormal(ni);
                    l->info(std::format("normals mapped by vertex? {}", n->GetMappingMode() != FbxLayerElement::EMappingMode::eByControlPoint));
                }
                for (int ni{0}; ni < num_uv_elements; ni++)
                {
                    const FbxGeometryElementUV* uv = fbx_mesh->GetElementUV(ni);
                    l->info(std::format("uvs mapped by vertex? {}", uv->GetMappingMode() != FbxLayerElement::EMappingMode::eByControlPoint));
                }

                const FbxVector4* mesh_vertices = fbx_mesh->GetControlPoints();

                int asset_vertex_index{0}; // This is going to be the indices that index into the resulting vertex list built by iterating over triangle vertices.

                // Iterating over the triangles in the mesh and will generate the final list of positions for the mesh and indices for the mesh.
                // This will also need to figure out the surfaces for the mesh.
                constexpr int num_vertices_in_triangle{3};

                new_asset_mesh.positions.reserve(vertices_count); // Need at least vertex count space + probably more.
                for (int triangle_index{0}; triangle_index < triangle_count; triangle_index++)
                {
                    // material_index is how we build our list of lists for the index buffer, this triangles vertex indices will be added to this material's list
                    // which will create the resulting index buffer.
                    int material_index = material_indices.GetAt(triangle_index);
                    std::vector<uint32_t>& asset_mat_indices = asset_materials_index_list[material_index];
                    l->info(std::format("rsy_mat_index {}", material_index));
                    for (int triangle_vertex = 0; triangle_vertex < num_vertices_in_triangle; triangle_vertex++)
                    {
                        const int mesh_vertex_index = fbx_mesh->GetPolygonVertex(triangle_index, triangle_vertex);
                        l->info(std::format("mesh_vertex_index {}", mesh_vertex_index));

                        rosy_asset::position p{};

                        {
                            FbxVector4 current_vertex = mesh_vertices[mesh_vertex_index];
                            float x = static_cast<float>(current_vertex[0]);
                            float y = static_cast<float>(current_vertex[1]);
                            float z = static_cast<float>(current_vertex[2]);
                            l->info(std::format("position: ({}, {}, {})", x, y, z));
                            p.vertex = {x, y, z};
                        }

                        {
                            FbxVector4 current_normal{};
                            fbx_mesh->GetPolygonVertexNormal(triangle_index, triangle_vertex, current_normal);
                            float x = static_cast<float>(current_normal[0]);
                            float y = static_cast<float>(current_normal[1]);
                            float z = static_cast<float>(current_normal[2]);
                            l->info(std::format("normal: ({}, {}, {})", x, y, z));
                            p.normal = {x, y, z};
                        }

                        {
                            FbxVector2 current_uv{};
                            const char* uv_name{nullptr};
                            bool uv_unmapped;
                            fbx_mesh->GetPolygonVertexUV(triangle_index, triangle_vertex, uv_name, current_uv, uv_unmapped);
                            if (uv_unmapped)
                            {
                                l->error(std::format("error no uvs: uv_unmapped: {}",  uv_unmapped));
                                return rosy::result::error;
                            }
                            float s = static_cast<float>(current_uv[0]);
                            float t = static_cast<float>(-current_uv[1]);
                            l->info(std::format("uv: ({}, {}) uv_unmapped: {}", s, t, uv_unmapped));
                            p.texture_coordinates = {s, t};
                        }
                        FbxColor vertex_color{};
                        bool has_color{false};
                        if (fbx_mesh->GetElementVertexColorCount())
                        {
                            for (int lc{0}; lc < fbx_mesh->GetElementVertexColorCount(); lc++)
                            {
                                switch (const FbxGeometryElementVertexColor* color = fbx_mesh->GetElementVertexColor(lc); color->GetMappingMode())
                                {
                                case FbxGeometryElement::eByControlPoint:
                                    l->info("color by control point");
                                    switch (color->GetReferenceMode())
                                    {
                                    case FbxGeometryElement::eDirect:
                                        vertex_color = color->GetDirectArray().GetAt(mesh_vertex_index);
                                        has_color = true;
                                        break;
                                    case FbxGeometryElement::eIndexToDirect:
                                        {
                                            const int id = color->GetIndexArray().GetAt(mesh_vertex_index);
                                            vertex_color = color->GetDirectArray().GetAt(id);
                                            has_color = true;
                                        }
                                        break;
                                    case FbxLayerElement::eIndex:
                                        break;
                                    }
                                    break;
                                case FbxGeometryElement::eByPolygonVertex:
                                    l->info("color by polygon vertex");
                                    switch (color->GetReferenceMode())
                                    {
                                    case FbxGeometryElement::eDirect:
                                        vertex_color = color->GetDirectArray().GetAt(mesh_vertex_index);
                                        has_color = true;
                                        break;
                                    case FbxGeometryElement::eIndexToDirect:
                                        {
                                            const int id = color->GetIndexArray().GetAt(mesh_vertex_index);
                                            vertex_color = color->GetDirectArray().GetAt(id);
                                            has_color = true;
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
                        }
                        if (has_color)
                        {
                            l->info(std::format("vertex color: ({}, {}, {}, {})", vertex_color.mRed, vertex_color.mGreen, vertex_color.mBlue, vertex_color.mAlpha));
                            p.color = {static_cast<float>(vertex_color.mRed), static_cast<float>(vertex_color.mGreen), static_cast<float>(vertex_color.mBlue), static_cast<float>(vertex_color.mAlpha)};
                        }
                        else
                        {
                            l->info("no vertex colors");
                        }

                        asset_mat_indices.push_back(asset_vertex_index);
                        asset_vertex_index += 1;
                        new_asset_mesh.positions.emplace_back(p);
                    }
                    l->info(std::format("num indices in material {} asset_vertex_index {}", asset_mat_indices.size(), asset_vertex_index));
                }
                new_asset_mesh.positions.shrink_to_fit();
                l->info(std::format("rsy_vertex_count {} ", asset_vertex_index));

                // Build surfaces and index list from list of lists
                size_t offset{0};
                new_asset_mesh.surfaces.reserve(asset_materials_index_list.size());
                new_asset_mesh.indices.reserve(new_asset_mesh.surfaces.size() * num_vertices_in_triangle); // the indices buffer is at the very least this big.
                for (const auto& mat_indices : asset_materials_index_list)
                {
                    size_t count = mat_indices.size();
                    if (count == 0) continue;
                    rosy_asset::surface s{};
                    s.material = asset_material_index;
                    s.start_index = static_cast<uint32_t>(offset);
                    s.count = static_cast<uint32_t>(count);
                    new_asset_mesh.indices.insert(new_asset_mesh.indices.end(), mat_indices.begin(), mat_indices.end());
                    offset += count;
                    new_asset_mesh.surfaces.emplace_back(s);
                }

                const size_t current_asset_mesh_index = fbx_asset.meshes.size();
                {
                    // add to meshes
                    optimize_mesh(l, new_asset_mesh);
                    fbx_asset.meshes.emplace_back(new_asset_mesh);
                }

                {
                    // Add to nodes
                    const size_t current_asset_node_index = fbx_asset.nodes.size();
                    rosy_asset::node new_asset_node{};
                    std::ranges::copy(node_name, std::back_inserter(new_asset_node.name));
                    new_asset_node.mesh_id = static_cast<uint32_t>(current_asset_mesh_index);
                    fbx_asset.scenes[0].nodes.emplace_back(static_cast<uint32_t>(current_asset_node_index));
                    fbx_asset.nodes.emplace_back(new_asset_node);
                }

                l->info("done with mesh");
            }
        }

        // Recursively print the children.
        for (int j = 0; j < p_node->GetChildCount(); j++)
        {
            traverse_node(l, cfg, p_node->GetChild(j), fbx_asset);
        }
        if (fbx_asset.meshes.empty())
        {
            l->error("no meshes found in fbx asset.");
            return rosy::result::error;
        }

        l->info("</node>\n");
        return rosy::result::ok;
    }
}


rosy::result fbx::import(const std::shared_ptr<rosy_logger::log>& l, [[maybe_unused]] fbx_config& cfg)
{
    const std::filesystem::path file_path{source_path};
    {
        constexpr glm::mat4 m{1.f};
        fbx_asset.asset_coordinate_system = mat4_to_array(m);
    }
    l->info("importing fbx starting");

    // IMAGES

    for (const std::filesystem::path parent_dir = file_path.parent_path(); const auto& entry : std::filesystem::directory_iterator(parent_dir))
    {
        const auto& entry_path = entry.path();
        if (entry_path.extension() != ".tga") continue; // FBX import only supports .tga images.
        bool is_image{false};
        std::string image_type{};
        for (std::array supported_image_types = {
                 std::string{"normal.tga"},
                 std::string{"mixmap.tga"},
                 std::string{"albedo.tga"}
             }; const auto& supported_type : supported_image_types)
        {
            if (const auto& fn = entry_path.filename().string(); !std::ranges::ends_with(fn, supported_type)) continue;
            image_type = supported_type;
            is_image = true;
            break;
        }
        if (!is_image) continue;
        l->info(std::format("image found: {}", entry_path.string()));

        rosy_asset::image img{};
        if (image_type == "normal.tga")
        {
            if (const auto res = generate_normal_map_texture(l, entry_path); res != rosy::result::ok)
            {
                l->error(std::format("error creating normal fbx image: {} for {}", static_cast<uint8_t>(res), entry_path.filename().string()));
                return res;
            }
            img.image_type = rosy_asset::image_type_normal_map;
        }
        if (image_type == "mixmap.tga")
        {
            if (const auto res = generate_srgb_texture(l, entry_path); res != rosy::result::ok)
            {
                l->error(std::format("error creating mixmap fbx image: {} for {}", static_cast<uint8_t>(res), entry_path.filename().string()));
                return res;
            }
            img.image_type = rosy_asset::image_type_mixmap;
        }
        if (image_type == "albedo.tga")
        {
            if (const auto res = generate_srgb_texture(l, entry_path); res != rosy::result::ok)
            {
                l->error(std::format("error creating albedo fbx image: {} for {}", static_cast<uint8_t>(res), entry_path.filename().string()));
                return res;
            }
            img.image_type = rosy_asset::image_type_color;
        }

        std::filesystem::path out_path{ entry_path };
        out_path.replace_extension(".dds");
        std::ranges::copy(out_path.string(), std::back_inserter(img.name));
        fbx_asset.images.push_back(img);
    }

    // FBX Extraction

    FbxManager* rsy_sdk_manager = FbxManager::Create();

    FbxIOSettings* ios = FbxIOSettings::Create(rsy_sdk_manager, IOSROOT);
    rsy_sdk_manager->SetIOSettings(ios);

    FbxImporter* rsy_importer = FbxImporter::Create(rsy_sdk_manager, "");

    if (const bool rsy_import_status = rsy_importer->Initialize(file_path.string().c_str(), -1, rsy_sdk_manager->GetIOSettings()); !rsy_import_status)
    {
        l->error("Call to FbxImporter::Initialize() failed.");
        l->error(std::format("Error returned: {}", rsy_importer->GetStatus().GetErrorString()));
        return rosy::result::read_failed;
    }
    l->info("importing fbx success?");

    FbxScene* rsy_scene = FbxScene::Create(rsy_sdk_manager, "myScene");

    int up_dir;
    rsy_scene->GetGlobalSettings().GetAxisSystem().GetUpVector(up_dir);
    switch (up_dir)
    {
    case FbxAxisSystem::EUpVector::eXAxis:
        l->info("fbx scene is x up");
        break;
    case FbxAxisSystem::EUpVector::eYAxis:
        l->info("fbx scene is y up");
        break;
    case FbxAxisSystem::EUpVector::eZAxis:
        l->info("fbx scene is z up");
        break;
    default:
        l->error("invalid up direction");
        return rosy::result::error;
    }


    if (const bool rsy_import_status = rsy_importer->Import(rsy_scene); !rsy_import_status)
    {
        l->error("Call to FbxImporter::Import() failed.");
        l->error(std::format("Error returned: {}", rsy_importer->GetStatus().GetErrorString()));
        return rosy::result::read_failed;
    }
    rsy_importer->Destroy();
    l->info("importing fbx scene success?");

    rosy_asset::scene default_scene{};
    fbx_asset.scenes.emplace_back(default_scene);
    fbx_asset.root_scene = 0;
    if (FbxNode* rsy_root_node = rsy_scene->GetRootNode())
    {
        for (int i = 0; i < rsy_root_node->GetChildCount(); i++)
            traverse_node(l, cfg, rsy_root_node->GetChild(i), fbx_asset);
    }
    else
    {
        l->info("no fbx root node?");
    }

    rsy_sdk_manager->Destroy();

    // TANGENTS

    if (cfg.use_mikktspace)
    {
        if (const auto res = generate_tangents(l, fbx_asset); res != rosy::result::ok)
        {
            l->error("Error generating fbx tangents");
            return res;
        }
    }

    l->info("all done importing fbx asset");
    return rosy::result::ok;
}
