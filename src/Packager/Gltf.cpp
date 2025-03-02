#include "Gltf.h"
#include <algorithm>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <iostream>
#include <nvtt/nvtt.h>
#include <mikktspace.h>

using namespace rosy_packager;

namespace
{
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

struct t_space_generator_context
{
    asset* gltf_asset{nullptr};
    size_t mesh_index;
};

// All faces are triangles below and are referred to as triangles instead of faces unless it is a mikktspace header field name.

int t_space_get_num_faces(const SMikkTSpaceContext* p_context) // NOLINT(misc-use-internal-linkage)
{
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const mesh m = ctx->gltf_asset->meshes[ctx->mesh_index];
    int num_triangles{0};
    for (const surface& s : m.surfaces)
    {
        // Each gltf primitive is referred to as a surface, it as an index offset and a count of indices, each three indices represents a triangle
        assert(s.count % 3 == 0); // There must only be triangles in a surface.
        const int num_triangles_in_surface = static_cast<int>(s.count) / 3;
        num_triangles += num_triangles_in_surface;
    }
    return num_triangles;
}

int t_space_get_num_vertices_of_face([[maybe_unused]] const SMikkTSpaceContext* p_context, [[maybe_unused]] const int requested_triangle) // NOLINT(misc-use-internal-linkage)
{
    // Hard code return 3 vertices per face as the faces are all triangles
    return 3;
}

size_t t_space_get_asset_position_data(const SMikkTSpaceContext* p_context, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage))
{
    size_t rv{};
    assert(requested_triangle_vertex < 3); // requested_triangle_vertex must be one of {0, 1, 2};
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];
    // Iterate over all surfaces and all of their triangle to find the current triangle for requested_triangle, derive the index and return its position.
    int num_triangles_visited{0};
    bool found{false};
    for (const surface& s : m.surfaces)
    {
        const auto num_indices_in_surface = static_cast<int>(s.count);
        const int num_triangles_in_surface = num_indices_in_surface / 3;
        if (const int last_triangle_in_surface = num_triangles_visited + num_triangles_in_surface; last_triangle_in_surface < requested_triangle)
        {
            // This loop must iterate further to find the surface with the requested triangle.
            num_triangles_visited += num_triangles_in_surface;
            continue;
        }
        // Each surface has a number of triangles, requested_triangle is an index into the list of all triangles given the sum of all triangles across all surfaces in the mesh.
        // num_triangles_visited is the number of triangles we have visited already, requested_triangle - num_triangles_visited should represent a triangle referenced by the current surface
        // given that requested_triangle is less than last_triangle_in_surface.
        const int surface_triangle = requested_triangle - num_triangles_visited;
        // surface_triangle is the index of the triangle in this surface, not across all the mesh's triangles
        assert(surface_triangle < num_triangles_in_surface);
        // The indices_index for the requested triangle's vertex begins at the surfaces.start_index + the nth number of triangle vertices + the index of the requested triangle's vertex which is one of {0, 1, 2}
        const size_t indices_index = static_cast<size_t>(s.start_index) + static_cast<size_t>(surface_triangle * 3) + static_cast<size_t>(requested_triangle_vertex);
        assert(m.indices.size() > indices_index); // Cannot reference an index larger than the number of indices available.
        rv = static_cast<size_t>(m.indices[indices_index]);
        found = true;
        break;
    }
    assert(found); // The requested triangle and vertex must have been found.
    return rv;
}

void t_space_get_position(const SMikkTSpaceContext* p_context, float* fv_pos_out, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const mesh m = ctx->gltf_asset->meshes[ctx->mesh_index];
    const position p = m.positions[position_index];
    *(fv_pos_out + 0) = p.vertex[0];
    *(fv_pos_out + 1) = p.vertex[1];
    *(fv_pos_out + 2) = p.vertex[2];
}

void t_space_get_normal(const SMikkTSpaceContext* p_context, float* fv_normal_out, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const mesh m = ctx->gltf_asset->meshes[ctx->mesh_index];
    const position p = m.positions[position_index];
    *(fv_normal_out + 0) = p.normal[0];
    *(fv_normal_out + 1) = p.normal[1];
    *(fv_normal_out + 2) = p.normal[2];
}

void t_space_get_texture_coordinates(const SMikkTSpaceContext* p_context, float* fv_text_coords_out, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const mesh m = ctx->gltf_asset->meshes[ctx->mesh_index];
    const position p = m.positions[position_index];
    *(fv_text_coords_out + 0) = p.texture_coordinates[0];
    *(fv_text_coords_out + 1) = p.texture_coordinates[1];
}

void t_space_set_tangent(const SMikkTSpaceContext* p_context, const float tangent[], const float sign, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];
    m.positions[position_index].tangents[0] = tangent[0];
    m.positions[position_index].tangents[1] = tangent[1];
    m.positions[position_index].tangents[2] = tangent[2];
    m.positions[position_index].tangents[3] = sign;
}

SMikkTSpaceInterface t_space_generator = { // NOLINT(misc-use-internal-linkage)
    .m_getNumFaces = t_space_get_num_faces,
    .m_getNumVerticesOfFace = t_space_get_num_vertices_of_face,
    .m_getPosition = t_space_get_position,
    .m_getNormal = t_space_get_normal,
    .m_getTexCoord = t_space_get_texture_coordinates,
    .m_setTSpaceBasic = t_space_set_tangent,
    .m_setTSpace = nullptr,
};

rosy::result gltf::import(rosy::log* l, gltf_config& cfg)
{
    const std::filesystem::path file_path{source_path};
    gltf_asset.asset_coordinate_system = {
        -1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
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

        l->info(std::format("Adding image: {}", gltf_img_name));
        image img{};

        std::filesystem::path img_path{gltf_asset.asset_path};
        pre_rename_image_paths.emplace_back(img_path);
        img_path.replace_filename(std::format("{}.dds", gltf_img_name));
        l->debug(std::format("source: {} path: {} name: {}", gltf_asset.asset_path, img_path.string(), gltf_img_name));

        l->info(std::format("Adding image: {}", gltf_img_name));
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
            material m{};
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
                    metallic_images.emplace_back(m.metallic_image_index, static_cast<uint32_t>(color_images.size()));
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
                fastgltf::math::nvec4 c = mat.pbrData.baseColorFactor;
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
    if (cfg.condition_images) {
        std::ranges::sort(color_images);
        auto last = std::ranges::unique(color_images).begin();
        color_images.erase(last, color_images.end());
        l->info(std::format("num color_images {}", color_images.size()));
        for (const auto& [gltf_index, rosy_index] : color_images)
        {
            // Declare it as a color image
            gltf_asset.images[gltf_index].image_type = image_type_color;
            const auto& gltf_img = gltf.images[gltf_index];
            if (!std::holds_alternative<fastgltf::sources::URI>(gltf_img.data))
            {
                l->error("not a URI datasource");
                return rosy::result::error;
            }

            const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_img.data);
            l->info(std::format("color image uri is: {}", uri_ds.uri.string()));
            const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

            auto& img_path = pre_rename_image_paths[rosy_index];
            l->info(std::format("{} is a gltf_img color image", gltf_img_name));
            l->info(std::format("{} is an image_path color image", img_path.string()));

            std::filesystem::path source_img_path{gltf_asset.asset_path};
            source_img_path.replace_filename(uri_ds.uri.string());
            l->info(std::format("source_img_path for color image is: {}", source_img_path.string()));

            {
                std::string input_filename{source_img_path.string()};

                nvtt::Surface image;
                if (!image.load(input_filename.c_str()))
                {
                    l->error(std::format("Failed to load file  for  {}", input_filename));
                    return rosy::result::error;
                }

                l->debug(std::format("image data is width: {} height: {} depth: {} type: {} for ", image.width(), image.height(), image.depth(), static_cast<uint8_t>(image.type()), input_filename));

                nvtt::Context context(true); // Enable CUDA

                nvtt::CompressionOptions compression_options;
                compression_options.setFormat(nvtt::Format_BC7);

                img_path.replace_filename(std::format("{}.dds", gltf_img_name));
                std::string output_filename = img_path.string();
                nvtt::OutputOptions output_options;
                output_options.setFileName(output_filename.c_str());

                int num_mipmaps = image.countMipmaps();

                l->info(std::format("num mip maps are {} for {}", num_mipmaps, input_filename));

                if (!context.outputHeader(image, num_mipmaps, compression_options, output_options))
                {
                    l->error(std::format("Writing dds headers failed for  {}", input_filename));
                    return rosy::result::error;
                }

                for (int mip = 0; mip < num_mipmaps; mip++)
                {
                    // Compress this image and write its data.
                    if (!context.compress(image, 0 /* face */, mip, compression_options, output_options))
                    {
                        l->error(std::format("Compressing and writing the dds file failed for  {}", input_filename));
                        return rosy::result::error;
                    }

                    if (mip == num_mipmaps - 1)
                    {
                        break;
                    }

                    image.toLinearFromSrgb();
                    image.premultiplyAlpha();

                    image.buildNextMipmap(nvtt::MipmapFilter_Box);

                    image.demultiplyAlpha();
                    image.toSrgb();
                }
            }
        }
    }
    if (cfg.condition_images) {
        std::ranges::sort(metallic_images);
        auto last = std::ranges::unique(metallic_images).begin();
        metallic_images.erase(last, metallic_images.end());
        l->info(std::format("num metallic_images {}", metallic_images.size()));
        for (const auto& [gltf_index, rosy_index] : metallic_images)
        {
            // Declare it as a metallic image
            gltf_asset.images[gltf_index].image_type = image_type_metallic_roughness;
            const auto& gltf_img = gltf.images[gltf_index];
            if (!std::holds_alternative<fastgltf::sources::URI>(gltf_img.data))
            {
                l->error("not a URI datasource");
                return rosy::result::error;
            }

            const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_img.data);
            l->info(std::format("metallic image uri is: {}", uri_ds.uri.string()));
            const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

            auto& img_path = pre_rename_image_paths[rosy_index];
            l->info(std::format("{} is a gltf_img metallic image", gltf_img_name));
            l->info(std::format("{} is an image_path metallic image", img_path.string()));

            std::filesystem::path source_img_path{gltf_asset.asset_path};
            source_img_path.replace_filename(uri_ds.uri.string());
            l->info(std::format("source_img_path for metallic image is: {}", source_img_path.string()));

            {
                std::string input_filename{source_img_path.string()};

                nvtt::Surface image;
                if (!image.load(input_filename.c_str()))
                {
                    l->error(std::format("Failed to load file  for  {}", input_filename));
                    return rosy::result::error;
                }

                l->info(std::format("image data is width: {} height: {} depth: {} type: {} for ",
                                    image.width(), image.height(), image.depth(), static_cast<uint8_t>(image.type()), input_filename));

                nvtt::Context context(true);

                nvtt::CompressionOptions compression_options;
                compression_options.setFormat(nvtt::Format_BC7);

                img_path.replace_filename(std::format("{}.dds", gltf_img_name));
                std::string output_filename = img_path.string();
                nvtt::OutputOptions output_options;
                output_options.setFileName(output_filename.c_str());

                int num_mipmaps = image.countMipmaps();

                l->info(std::format("num mip maps are {} for {}", num_mipmaps, input_filename));

                if (!context.outputHeader(image, num_mipmaps, compression_options, output_options))
                {
                    l->error(std::format("Writing dds headers failed for  {}", input_filename));
                    return rosy::result::error;
                }

                for (int mip = 0; mip < num_mipmaps; mip++)
                {
                    // Compress this image and write its data.
                    if (!context.compress(image, 0 /* face */, mip, compression_options, output_options))
                    {
                        l->error(std::format("Compressing and writing the dds file failed for  {}", input_filename));
                        return rosy::result::error;
                    }

                    if (mip == num_mipmaps - 1)
                    {
                        break;
                    }

                    image.toLinearFromSrgb();
                    image.premultiplyAlpha();

                    image.buildNextMipmap(nvtt::MipmapFilter_Box);

                    image.demultiplyAlpha();
                    image.toSrgb();
                }
            }
        }
    }
    if (cfg.condition_images) {
        std::ranges::sort(normal_map_images);
        auto last = std::ranges::unique(normal_map_images).begin();
        normal_map_images.erase(last, normal_map_images.end());
        l->info(std::format("num normal_map_images {}", normal_map_images.size()));

        for (const auto& [gltf_index, rosy_index] : normal_map_images)
        {
            // Declare it as a normal map image
            gltf_asset.images[gltf_index].image_type = image_type_normal_map;
            const auto& gltf_img = gltf.images[gltf_index];
            if (!std::holds_alternative<fastgltf::sources::URI>(gltf_img.data))
            {
                l->error("not a URI datasource");
                return rosy::result::error;
            }

            const fastgltf::sources::URI uri_ds = std::get<fastgltf::sources::URI>(gltf_img.data);
            l->info(std::format("normal image uri is: {}", uri_ds.uri.string()));
            const std::string gltf_img_name{uri_ds.uri.string().substr(0, uri_ds.uri.string().find('.'))};

            auto& img_path = pre_rename_image_paths[rosy_index];
            l->info(std::format("{} is a gltf_img normal image", gltf_img_name));
            l->info(std::format("{} is an image_path normal image", img_path.string()));

            std::filesystem::path source_img_path{gltf_asset.asset_path};
            source_img_path.replace_filename(uri_ds.uri.string());
            l->info(std::format("source_img_path for normal image is: {}", source_img_path.string()));

            {
                std::string input_filename{source_img_path.string()};

                nvtt::Surface image;
                if (!image.load(input_filename.c_str()))
                {
                    l->error(std::format("Failed to open {}", input_filename));
                    return rosy::result::error;
                }

                l->info(std::format("image data is width: {} height: {} depth: {} type: {} for ",
                                    image.width(), image.height(), image.depth(), static_cast<uint8_t>(image.type()), input_filename));

                nvtt::Context context(true); // Enable CUDA

                nvtt::CompressionOptions compression_options;
                compression_options.setFormat(nvtt::Format_BC5);

                img_path.replace_filename(std::format("{}.dds", gltf_img_name));
                std::string output_filename = img_path.string();
                nvtt::OutputOptions output_options;
                output_options.setFileName(output_filename.c_str());

                int num_mipmaps = image.countMipmaps();
                l->info(std::format("num mip maps are {} for {}", num_mipmaps, input_filename));

                if (!context.outputHeader(image, num_mipmaps, compression_options, output_options))
                {
                    l->error(std::format("Writing dds headers failed for  {}", input_filename));
                    return rosy::result::error;
                }

                for (int mip = 0; mip < num_mipmaps; mip++)
                {
                    image.normalizeNormalMap();
                    nvtt::Surface temp = image;
                    temp.transformNormals(nvtt::NormalTransform_Orthographic);
                    // Compress this image and write its data.
                    if (!context.compress(temp, 0 /* face */, mip, compression_options, output_options))
                    {
                        l->error(std::format("Compressing and writing the dds file failed for  {}", input_filename));
                        return rosy::result::error;
                    }

                    if (mip == num_mipmaps - 1)
                    {
                        break;
                    }
                    image.buildNextMipmap(nvtt::MipmapFilter_Box);
                }
            }
        }
    }

    // SAMPLERS

    {
        for (fastgltf::Sampler& gltf_sampler : gltf.samplers)
        {
            sampler smp{};
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
        mesh new_mesh{};
        for (auto& primitive : fast_gltf_mesh.primitives)
        {
            // PRIMITIVE SURFACE
            float min = std::numeric_limits<float>::min();
            float max = std::numeric_limits<float>::max();
            std::array<float, 3> min_bounds{max, max, max};
            std::array<float, 3> max_bounds{min, min, min};
            surface new_surface{};
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
                position new_position{};
                for (size_t i{0}; i < 3; i++)
                {
                    min_bounds[i] = std::min(
                        v[i], min_bounds[i]);
                    max_bounds[i] = std::max(
                        v[i], max_bounds[i]);
                }
                new_position.vertex = {v[0], v[1], v[2]};
                new_position.normal = {1.0f, 0.0f, 0.0f};
                new_position.tangents = {1.0f, 0.0f, 0.0f};
                new_mesh.positions[initial_vtx + index] =
                    new_position;
            });

            // PRIMITIVE NORMAL
            if (const auto normals = primitive.findAttribute("NORMAL"); normals != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normals->accessorIndex], [&](const fastgltf::math::fvec3& n, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].
                        normal = {n[0], n[1], n[2]};
                });
            }

            // ReSharper disable once StringLiteralTypo
            if (auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uv->accessorIndex], [&](const fastgltf::math::fvec2& tc, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].
                        texture_coordinates = {tc[0], tc[1]};
                });
            }

            // PRIMITIVE COLOR
            if (auto colors = primitive.findAttribute("COLOR_0"); colors != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[colors->accessorIndex], [&](const fastgltf::math::fvec4& c, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].
                        color = {c[0], c[1], c[2], c[3]};
                });
            }

            // PRIMITIVE TANGENT
            if (auto tangents = primitive.findAttribute("TANGENT"); tangents != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangents->accessorIndex], [&](const fastgltf::math::fvec4& t, const size_t index)
                {
                    new_mesh.positions[initial_vtx + index].
                        tangents = {t[0], t[1], t[2], t[3]};
                });
            }

            // PRIMITIVE MATERIAL
            if (primitive.materialIndex.has_value())
            {
                new_surface.material = static_cast<uint32_t>(primitive.materialIndex.value());
            }
            else
            {
                new_surface.material = 0;
            }
            new_surface.min_bounds = min_bounds;
            new_surface.max_bounds = max_bounds;
            new_mesh.surfaces.push_back(new_surface);
        }
        gltf_asset.meshes.push_back(new_mesh);
    }

    gltf_asset.scenes.reserve(gltf.scenes.size());
    for (auto& [nodeIndices, name] : gltf.scenes)
    {
        scene s{};
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
        node n{};
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

    for (size_t mesh_index{0}; mesh_index < gltf_asset.meshes.size(); mesh_index++)
    {
        l->info(std::format("generating tangent for mesh at index {}", mesh_index));
        t_space_generator_context t_ctx{
            .gltf_asset = &gltf_asset,
            .mesh_index = mesh_index
        };
        SMikkTSpaceContext s_mikktspace_ctx{
            .m_pInterface = &t_space_generator,
            .m_pUserData = static_cast<void*>(&t_ctx),
        };
        if (!genTangSpaceDefault(&s_mikktspace_ctx))
        {
            l->error(std::format("Error generating tangents for mesh at index {}", mesh_index));
            return rosy::result::error;
        }
    }
    return rosy::result::ok;
}
