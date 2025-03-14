#include "pch.h"
#include "Packager.h"

#include <meshoptimizer.h>
#include <mikktspace.h>
#include <glm/vec4.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <mikktspace.h>

using namespace rosy_packager;


struct t_space_surface_map
{
    size_t surface_index{ 0 }; // The surface's index in the gltf asset's mesh vector
    int triangle_count_offset{ 0 }; // The number of triangles that precede this surfaces triangles
    int current_triangle{ 0 }; // The current triangle's index within the surface.
};

struct t_space_generator_context
{
    rosy_asset::asset* gltf_asset{ nullptr };
    std::vector<t_space_surface_map> triangle_surface_map;
    size_t mesh_index{ 0 };
    size_t surface_index{ 0 };
    int num_triangles{ 0 };
    std::shared_ptr<rosy_logger::log> l;
};

// All faces are triangles below and are referred to as triangles instead of faces unless it is a mikktspace header field name.

void t_space_gen_num_faces(t_space_generator_context& ctx) // NOLINT(misc-use-internal-linkage)
{
    const rosy_asset::mesh& m = ctx.gltf_asset->meshes[ctx.mesh_index];
    const size_t surface_index = ctx.surface_index;
    const rosy_asset::surface& s = m.surfaces[surface_index];
    int num_triangles{ 0 };
    const int num_triangles_in_surface = static_cast<int>(s.count) / 3;
    ctx.triangle_surface_map.reserve(ctx.triangle_surface_map.size() + num_triangles_in_surface);
    for (int i{ 0 }; i < num_triangles_in_surface; i++)
    {
        t_space_surface_map sm{
            .surface_index = surface_index,
            .triangle_count_offset = num_triangles,
            .current_triangle = i,
        };
        ctx.triangle_surface_map.emplace_back(sm);
    }
    num_triangles += num_triangles_in_surface;
    ctx.num_triangles = num_triangles;
}

int t_space_get_num_faces(const SMikkTSpaceContext* p_context) // NOLINT(misc-use-internal-linkage)
{
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    return ctx->num_triangles;
}

int t_space_get_num_vertices_of_face([[maybe_unused]] const SMikkTSpaceContext* p_context, [[maybe_unused]] const int requested_triangle) // NOLINT(misc-use-internal-linkage)
{
    // Hard code return 3 vertices per face as the faces are all triangles
    return 3;
}

size_t t_space_get_asset_position_data(const SMikkTSpaceContext* p_context, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage))
{
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const rosy_asset::mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];

    const t_space_surface_map& sm = ctx->triangle_surface_map[static_cast<size_t>(requested_triangle)];

    const rosy_asset::surface& s = m.surfaces[sm.surface_index];

    const int surface_triangle = sm.current_triangle;


    const size_t indices_index = static_cast<size_t>(s.start_index) + static_cast<size_t>(surface_triangle * 3) + static_cast<size_t>(requested_triangle_vertex);
    return m.indices[indices_index];
}

void t_space_get_position(const SMikkTSpaceContext* p_context, float* fv_pos_out, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const rosy_asset::mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];
    const rosy_asset::position& p = m.positions[position_index];
    std::memcpy(fv_pos_out, p.vertex.data(), sizeof(std::array<float, 3>));
}

void t_space_get_normal(const SMikkTSpaceContext* p_context, float* fv_normal_out, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const rosy_asset::mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];
    const rosy_asset::position& p = m.positions[position_index];
    std::memcpy(fv_normal_out, p.normal.data(), sizeof(std::array<float, 3>));
}

void t_space_get_texture_coordinates(const SMikkTSpaceContext* p_context, float* fv_text_coords_out, const int requested_triangle, const int requested_triangle_vertex) // NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    const rosy_asset::mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];
    const rosy_asset::position& p = m.positions[position_index];
    std::memcpy(fv_text_coords_out, p.texture_coordinates.data(), sizeof(std::array<float, 2>));
}

// Given only a normal vector, finds a valid tangent.
//
// This uses the technique from "Improved accuracy when building an orthonormal
// basis" by Nelson Max, https://jcgt.org/published/0006/01/02.
// Any tangent-generating algorithm must produce at least one discontinuity
// when operating on a sphere (due to the hairy ball theorem); this has a
// small ring-shaped discontinuity at normal.z == -0.99998796.
// via https://github.com/nvpro-samples/nvpro_core
static glm::vec4 make_fast_space_fast_tangent(const glm::vec3& n)
{
    if (n[2] < -0.99998796f) // Handle the singularity
    {
        return glm::vec4(0.f, -1.f, 0.f, 1.f);
    }
    const float a = 1.f / (1.f + n[2]);
    const float b = -n[0] * n[1] * a;
    return glm::vec4(1.f - n[0] * n[0] * a, b, -n[0], 1.f);
}

void t_space_set_tangent(const SMikkTSpaceContext* p_context, const float new_tangent[], const float new_sign, const int requested_triangle, const int requested_triangle_vertex)
// NOLINT(misc-use-internal-linkage)
{
    const size_t position_index = t_space_get_asset_position_data(p_context, requested_triangle, requested_triangle_vertex);
    const auto ctx = static_cast<t_space_generator_context*>(p_context->m_pUserData);
    rosy_asset::mesh& m = ctx->gltf_asset->meshes[ctx->mesh_index];

    std::array<float, 3> n = m.positions[position_index].normal;
    float sign = new_sign;
    const auto normal = glm::vec3{ n[0], n[1], n[2] };
    glm::vec3 tangent = { new_tangent[0], new_tangent[1], new_tangent[2] };
    if (std::abs(glm::dot(tangent, normal)) < 0.9f)
    {
        tangent = { new_tangent[0], new_tangent[1], new_tangent[2] };
        sign = -sign;
        m.positions[position_index].tangents[0] = tangent[0];
        m.positions[position_index].tangents[1] = tangent[1];
        m.positions[position_index].tangents[2] = tangent[2];
        m.positions[position_index].tangents[3] = sign;
    }
    else
    {
        glm::vec4 fast_tangent = make_fast_space_fast_tangent(normal);
        m.positions[position_index].tangents[0] = fast_tangent[0];
        m.positions[position_index].tangents[1] = fast_tangent[1];
        m.positions[position_index].tangents[2] = fast_tangent[2];
        m.positions[position_index].tangents[3] = fast_tangent[3];
    }
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

void rosy_packager::optimize_mesh(const std::shared_ptr<rosy_logger::log>& l, rosy_asset::mesh& asset_mesh)
{
    size_t total_vertices = asset_mesh.positions.size();
    const size_t total_indices = asset_mesh.indices.size();
    l->info(std::format("optimize-mesh: starting vertices count: {}", total_vertices));
    std::vector<unsigned int> remap(total_indices);
    size_t new_vertices_count = meshopt_generateVertexRemap(remap.data(), asset_mesh.indices.data(), total_indices, asset_mesh.positions.data(), total_vertices, sizeof(rosy_asset::position));
    l->info(std::format("optimize-mesh: new vertices count: {}", new_vertices_count));

    std::vector<uint32_t> optimized_indices(total_indices);
    meshopt_remapIndexBuffer(optimized_indices.data(), asset_mesh.indices.data(), total_indices, remap.data());
    asset_mesh.indices = std::move(optimized_indices);

    std::vector<rosy_asset::position> optimized_positions(new_vertices_count);
    meshopt_remapVertexBuffer(optimized_positions.data(), asset_mesh.positions.data(), total_vertices, sizeof(rosy_asset::position), remap.data());
    asset_mesh.positions = std::move(optimized_positions);
}

rosy::result rosy_packager::generate_tangents(const std::shared_ptr<rosy_logger::log>& l, rosy_asset::asset& asset)
{
    l->info("generating tangents...");
    for (size_t mesh_index{ 0 }; mesh_index < asset.meshes.size(); mesh_index++)
    {
        for (size_t surface_index{ 0 }; surface_index < asset.meshes[mesh_index].surfaces.size(); surface_index++)
        {
            t_space_generator_context t_ctx{
                .gltf_asset = &asset,
                .triangle_surface_map = {},
                .mesh_index = mesh_index,
                .surface_index = surface_index,
                .num_triangles = 0,
                .l = l,
            };
            t_space_gen_num_faces(t_ctx);
            SMikkTSpaceContext s_mikktspace_ctx{
                .m_pInterface = &t_space_generator,
                .m_pUserData = static_cast<void*>(&t_ctx),
            };
            if (!genTangSpace(&s_mikktspace_ctx, 0.5f))
            {
                l->error(std::format("Error generating tangents for mesh at index {}", mesh_index));
                return rosy::result::error;
            }
        }
    }
    return rosy::result::ok;
}
