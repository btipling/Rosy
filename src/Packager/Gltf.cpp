#include "pch.h"
#include "Gltf.h"
#include <algorithm>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <nvtt/nvtt.h>
#include "Packager.h"
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

    uint16_t filter_to_val(const fastgltf::Filter filter)
    {
        switch (filter)
        {
        case fastgltf::Filter::Nearest:
            return 0;
        case fastgltf::Filter::NearestMipMapNearest:
            return 1;
        case fastgltf::Filter::LinearMipMapNearest:
            return 2;
        case fastgltf::Filter::Linear:
            return 3;
        case fastgltf::Filter::NearestMipMapLinear:
            return 4;
        case fastgltf::Filter::LinearMipMapLinear:
            return 5;
        }
        return UINT16_MAX;
    }

    uint16_t wrap_to_val(const fastgltf::Wrap wrap)
    {
        switch (wrap)
        {
        case fastgltf::Wrap::ClampToEdge:
            return 0;
        case fastgltf::Wrap::MirroredRepeat:
            return 1;
        case fastgltf::Wrap::Repeat:
            return 2;
        }
        return UINT16_MAX;
    }
}

rosy::result gltf::import(std::shared_ptr<rosy_logger::log>& l, gltf_config& cfg)
{
    const std::filesystem::path file_path{source_path};
    {
        constexpr glm::mat4 m{1.f};
        gltf_asset.asset_coordinate_system = mat4_to_array(m);
    }
    constexpr auto gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DecomposeNodeMatrices;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{fastgltf::Extensions::KHR_lights_punctual};
    auto data = fastgltf::GltfDataBuffer::FromPath(file_path);
    if (data.error() != fastgltf::Error::None)
    {
        auto err = fastgltf::to_underlying(data.error());
        l->error(std::format("import - failed to open {}, {}", source_path, err));
        return rosy::result::error;
    }
    auto asset = parser.loadGltf(data.get(), file_path.parent_path(), gltf_options);
    if (asset)
    {
        gltf = std::move(asset.get());
    }
    else
    {
        auto err = fastgltf::to_underlying(asset.error());
        l->error(std::format("import - failed to load {}, {}", source_path, err));
        return rosy::result::error;
    }

    // IMAGES
    std::vector<std::filesystem::path> pre_rename_image_paths;
    for (auto& [gltf_image_data, gltf_image_name] : gltf.images)
    {
        const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_image_data);
        const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

        rosy_asset::image img{};

        std::filesystem::path img_path{gltf_asset.asset_path};
        pre_rename_image_paths.emplace_back(img_path);
        img_path.replace_filename(std::format("{}.dds", gltf_img_name));
        std::ranges::copy(img_path.string(), std::back_inserter(img.name));
        gltf_asset.images.push_back(img);
    }

    // MATERIALS

    std::vector<std::tuple<uint32_t, uint32_t>> normal_map_images;
    std::vector<std::tuple<uint32_t, uint32_t>> metallic_images;
    std::vector<std::tuple<uint32_t, uint32_t>> color_images;
    {
        for (fastgltf::Material& mat : gltf.materials)
        {
            rosy_asset::material m{};
            if (mat.pbrData.baseColorTexture.has_value())
            {
                if (gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.has_value())
                {
                    m.color_image_index = static_cast<uint32_t>(gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value());
                    color_images.emplace_back(m.color_image_index, static_cast<uint32_t>(color_images.size()));
                }
                else
                {
                    m.color_image_index = UINT32_MAX;
                }
                if (gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.has_value())
                {
                    m.color_sampler_index = static_cast<uint32_t>(gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value());
                }
                else
                {
                    m.color_sampler_index = UINT32_MAX;
                }
            }
            if (mat.pbrData.metallicRoughnessTexture.has_value())
            {
                if (gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.has_value())
                {
                    m.metallic_image_index = static_cast<uint32_t>(gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value());
                    metallic_images.emplace_back(m.metallic_image_index, static_cast<uint32_t>(metallic_images.size()));
                }
                else
                {
                    m.metallic_image_index = UINT32_MAX;
                }
                if (gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.has_value())
                {
                    m.metallic_sampler_index = static_cast<uint32_t>(gltf.textures[mat.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value());
                }
                else
                {
                    m.metallic_sampler_index = UINT32_MAX;
                }
            }
            if (mat.normalTexture.has_value())
            {
                if (gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.has_value())
                {
                    m.normal_image_index = static_cast<uint32_t>(gltf.textures[mat.normalTexture.value().textureIndex].imageIndex.value());
                    normal_map_images.emplace_back(m.normal_image_index, static_cast<uint32_t>(normal_map_images.size()));
                }
                else
                {
                    m.normal_image_index = UINT32_MAX;
                }
                if (gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.has_value())
                {
                    m.normal_sampler_index = static_cast<uint32_t>(gltf.textures[mat.normalTexture.value().textureIndex].samplerIndex.value());
                }
                else
                {
                    m.normal_sampler_index = UINT32_MAX;
                }
            }
            else
            {
                m.normal_image_index = UINT32_MAX;
                m.normal_sampler_index = UINT32_MAX;
            }
            {
                const auto c = mat.pbrData.baseColorFactor;
                m.base_color_factor = {c[0], c[1], c[2], c[3]};
                m.double_sided = mat.doubleSided;
                m.metallic_factor = mat.pbrData.metallicFactor;
                m.roughness_factor = mat.pbrData.roughnessFactor;
                m.alpha_mode = static_cast<uint8_t>(mat.alphaMode);
                m.alpha_cutoff = mat.alphaCutoff;
            }
            gltf_asset.materials.push_back(m);
        }
    }
    if (cfg.condition_images)
    {
        // Color images
        std::ranges::sort(color_images);
        auto last = std::ranges::unique(color_images).begin();
        color_images.erase(last, color_images.end());
        l->info(std::format("num color_images {}", color_images.size()));
        for (const auto& [gltf_index, rosy_index] : color_images)
        {
            // Declare it as a color image
            gltf_asset.images[gltf_index].image_type = rosy_asset::image_type_color;
            const auto& gltf_img = gltf.images[gltf_index];
            if (!std::holds_alternative<fastgltf::sources::URI>(gltf_img.data))
            {
                l->error("not a URI datasource");
                return rosy::result::error;
            }

            const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_img.data);
            const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

            std::filesystem::path source_img_path{gltf_asset.asset_path};
            source_img_path.replace_filename(uri_ds.uri.string());
            if (const auto res = generate_srgb_texture(l, source_img_path); res != rosy::result::ok)
            {
                l->info(std::format("error creating color gltf image: {}", static_cast<uint8_t>(res)));
                return res;
            }
        }
        // Metallic images
        std::ranges::sort(metallic_images);
        last = std::ranges::unique(metallic_images).begin();
        metallic_images.erase(last, metallic_images.end());
        l->info(std::format("num metallic_images {}", metallic_images.size()));
        for (const auto& [gltf_index, rosy_index] : metallic_images)
        {
            // Declare it as a metallic image
            gltf_asset.images[gltf_index].image_type = rosy_asset::image_type_metallic_roughness;
            const auto& gltf_img = gltf.images[gltf_index];
            if (!std::holds_alternative<fastgltf::sources::URI>(gltf_img.data))
            {
                l->error("not a URI datasource");
                return rosy::result::error;
            }

            const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_img.data);
            const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

            std::filesystem::path source_img_path{gltf_asset.asset_path};
            source_img_path.replace_filename(uri_ds.uri.string());
            if (const auto res = generate_srgb_texture(l, source_img_path); res != rosy::result::ok)
            {
                l->info(std::format("error creating metallic gltf image: {}", static_cast<uint8_t>(res)));
                return res;
            }
        }
        // Normal maps
        std::ranges::sort(normal_map_images);
        last = std::ranges::unique(normal_map_images).begin();
        normal_map_images.erase(last, normal_map_images.end());
        l->info(std::format("num normal_map_images {}", normal_map_images.size()));

        for (const auto& [gltf_index, rosy_index] : normal_map_images)
        {
            // Declare it as a normal map image
            gltf_asset.images[gltf_index].image_type = rosy_asset::image_type_normal_map;
            const auto& gltf_img = gltf.images[gltf_index];
            if (!std::holds_alternative<fastgltf::sources::URI>(gltf_img.data))
            {
                l->error("not a URI datasource");
                return rosy::result::error;
            }

            const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_img.data);
            const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

            std::filesystem::path source_img_path{gltf_asset.asset_path};
            source_img_path.replace_filename(uri_ds.uri.string());
            if (const auto res = generate_normal_map_texture(l, source_img_path); res != rosy::result::ok)
            {
                l->info(std::format("error creating normal gltf image: {}", static_cast<uint8_t>(res)));
                return res;
            }
        }
    }

    // SAMPLERS

    {
        for (fastgltf::Sampler& gltf_sampler : gltf.samplers)
        {
            rosy_asset::sampler smp{};
            if (gltf_sampler.magFilter.has_value())
            {
                smp.mag_filter = filter_to_val(gltf_sampler.magFilter.value());
            }
            else
            {
                smp.mag_filter = UINT16_MAX;
            }
            if (gltf_sampler.minFilter.has_value())
            {
                smp.min_filter = filter_to_val(gltf_sampler.minFilter.value());
            }
            else
            {
                smp.min_filter = UINT16_MAX;
            }
            smp.wrap_s = wrap_to_val(gltf_sampler.wrapS);
            smp.wrap_t = wrap_to_val(gltf_sampler.wrapT);
            gltf_asset.samplers.push_back(smp);
        }
    }

    // MESHES


    for (fastgltf::Mesh& fast_gltf_mesh : gltf.meshes)
    {
        rosy_asset::mesh new_mesh{};
        for (auto& primitive : fast_gltf_mesh.primitives)
        {
            // PRIMITIVE SURFACE
            float min = std::numeric_limits<float>::min();
            float max = std::numeric_limits<float>::max();
            std::array<float, 3> min_bounds{max, max, max};
            std::array<float, 3> max_bounds{min, min, min};
            rosy_asset::surface new_surface{};
            new_surface.start_index = static_cast<uint32_t>(new_mesh.indices.size());
            new_surface.count = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

            const auto position_it = primitive.findAttribute("POSITION");
            auto& pos_accessor = gltf.accessors[position_it->accessorIndex];

            uint32_t initial_vtx = static_cast<uint32_t>(new_mesh.positions.size());
            new_mesh.positions.resize(new_mesh.positions.size() + pos_accessor.count);

            // PRIMITIVE INDEX
            {
                std::array<size_t, 3> triangle{};
                int current_index{0};
                fastgltf::Accessor& index_accessor = gltf.accessors[primitive.indicesAccessor.value()];
                new_mesh.indices.reserve(new_mesh.indices.size() + index_accessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor, [&](const std::uint32_t idx)
                {
                    new_mesh.indices.push_back(idx + initial_vtx);
                    {
                        // Track triangles for tangent calculations later.
                        triangle[current_index] = idx;
                        current_index += 1;
                        if (current_index == 3)
                        {
                            triangle = {0, 0, 0};
                            current_index = 0;
                        }
                    }
                });
            }

            // PRIMITIVE VERTEX
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, pos_accessor, [&](const fastgltf::math::fvec3& v, const size_t index)
            {
                rosy_asset::position new_position{};
                for (size_t i{0}; i < 3; i++)
                {
                    min_bounds[i] = std::min(v[i], min_bounds[i]);
                    max_bounds[i] = std::max(v[i], max_bounds[i]);
                }
                new_position.vertex = {v[0], v[1], v[2]};
                new_position.normal = {1.0f, 0.0f, 0.0f};
                new_position.tangents = {1.0f, 0.0f, 0.0f};
                new_mesh.positions[initial_vtx + index] = new_position;
            });

            // PRIMITIVE NORMAL
            if (const auto normals = primitive.findAttribute("NORMAL"); normals != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](const fastgltf::math::fvec3& n, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].normal = {n[0], n[1], n[2]};
                });
            }

            // ReSharper disable once StringLiteralTypo
            if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uv->accessorIndex], [&](const fastgltf::math::fvec2& tc, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].texture_coordinates = {tc[0], tc[1]};
                });
            }

            // PRIMITIVE COLOR
            if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](const fastgltf::math::fvec4& c, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].color = {c[0], c[1], c[2], c[3]};
                });
            }

            // PRIMITIVE TANGENT
            if (auto tangents = primitive.findAttribute("TANGENT"); tangents != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangents->accessorIndex], [&](const fastgltf::math::fvec4& t, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].tangents = {t[0], t[1], t[2], t[3]};
                });
            }

            // PRIMITIVE MATERIAL
            if (primitive.materialIndex.has_value())
            {
                new_surface.material = static_cast<uint32_t>(primitive.materialIndex.value());
            }
            else
            {
                new_surface.material = UINT32_MAX;
            }
            new_surface.min_bounds = min_bounds;
            new_surface.max_bounds = max_bounds;
            new_mesh.surfaces.push_back(new_surface);
        }

        {
            optimize_mesh(l, new_mesh);
        }

        gltf_asset.meshes.push_back(new_mesh);
    }

    gltf_asset.scenes.reserve(gltf.scenes.size());
    for (auto& [nodeIndices, name] : gltf.scenes)
    {
        rosy_asset::scene s{};
        s.nodes.reserve(nodeIndices.size());
        for (size_t node_index : nodeIndices)
        {
            s.nodes.push_back(static_cast<uint32_t>(node_index));
        }
        gltf_asset.scenes.push_back(s);
    }

    gltf_asset.nodes.reserve(gltf.nodes.size());
    for (auto& gltf_node : gltf.nodes)
    {
        rosy_asset::node n{};
        n.mesh_id = static_cast<uint32_t>(gltf_node.meshIndex.value_or(SIZE_MAX));
        std::ranges::copy(gltf_node.name, std::back_inserter(n.name));

        auto& [tr, ro, sc] = std::get<fastgltf::TRS>(gltf_node.transform);

        const auto tm = translate(fastgltf::math::fmat4x4(1.0f), tr);
        const auto rm = fastgltf::math::fmat4x4(asMatrix(ro));
        const auto sm = scale(fastgltf::math::fmat4x4(1.0f), sc);
        const auto transform = tm * rm * sm;

        for (size_t i{0}; i < 4; i++)
        {
            for (size_t j{0}; j < 4; j++)
            {
                n.transform[i * 4 + j] = transform[i][j];
            }
        }
        n.child_nodes.reserve(gltf_node.children.size());
        for (size_t node_index : gltf_node.children)
        {
            n.child_nodes.push_back(static_cast<uint32_t>(node_index));
        }
        gltf_asset.nodes.push_back(n);
    }

    if (cfg.use_mikktspace)
    {
        if (const auto res = generate_tangents(l, gltf_asset); res != rosy::result::ok)
        {
            l->error("Error generating gltf tangents");
            return res;
        }
    }
    return rosy::result::ok;
}
