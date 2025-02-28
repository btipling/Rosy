#include "Node.h"
#include <format>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/quaternion.hpp>

using namespace rosy;

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

    std::array<float, 9> mat3_to_array(glm::mat3 m)
    {
        std::array<float, 9> a{
            1.f, 0.f, 0.f,
            0.f, 1.f, 0.f,
            0.f, 0.f, 1.f,
        };
        const auto pos_r = glm::value_ptr(m);
        for (size_t i{ 0 }; i < 9; i++) a[i] = pos_r[i];
        return a;
    }

    glm::mat4 array_to_mat4(const std::array<float, 16>& a)
    {
        glm::mat4 m{};
        const auto pos_r = glm::value_ptr(m);
        for (uint64_t i{0}; i < 16; i++) pos_r[i] = a[i];
        return m;
    }

    std::array<float, 4> vec4_to_array(glm::vec4 v)
    {
        std::array<float, 4> a{};
        const auto pos_r = glm::value_ptr(v);
        for (uint64_t i{0}; i < 4; i++) a[i] = pos_r[i];
        return a;
    }

    glm::vec3 array_to_vec3(std::array<float, 3> a)
    {
        glm::vec3 v{};
        const auto pos_r = glm::value_ptr(v);
        for (uint64_t i{0}; i < 3; i++) pos_r[i] = a[i];
        return v;
    }
}

struct node_state
{
    rosy::log* l{nullptr};
    bool is_world_node{false};
    glm::mat4 object_to_world_transform{};
    glm::mat4 object_space_parent_transform{};
    glm::mat4 object_space_transform{};
    glm::vec3 world_space_translate{};
    node_bounds object_space_bounds{};
    float world_space_scale{1.f};
    float world_space_yaw{0.f};

    void init(
        rosy::log* new_log,
        const bool new_is_world_node,
        const std::array<float, 16>& coordinate_space_transform,
        const std::array<float, 16>& new_object_space_parent_transform,
        const std::array<float, 16>& new_object_space_transform,
        const std::array<float, 3> new_world_space_translate,
        const float new_world_space_scale,
        const float new_world_space_yaw
    )
    {
        l = new_log;
        is_world_node = new_is_world_node;
        object_to_world_transform = array_to_mat4(coordinate_space_transform);
        object_space_parent_transform = array_to_mat4(new_object_space_parent_transform);
        object_space_transform = array_to_mat4(new_object_space_transform);
        world_space_translate = array_to_vec3(new_world_space_translate);
        world_space_scale = new_world_space_scale;
        world_space_yaw = new_world_space_yaw;
    }

    [[nodiscard]] glm::mat4 get_world_space_transform() const
    {
        const glm::mat4 t = translate(glm::mat4(1.f), world_space_translate);
        const glm::mat4 r = toMat4(angleAxis(world_space_yaw, glm::vec3{0.f, -1.f, 0.f}));
        const glm::mat4 s = scale(glm::mat4(1.f), glm::vec3(world_space_scale, world_space_scale, world_space_scale));

        return t * r * s * object_to_world_transform * object_space_parent_transform * object_space_transform;
    }

    [[nodiscard]] glm::mat4 get_object_space_transform() const
    {
        return object_space_parent_transform * object_space_transform;
    }
};

result node::init(
    rosy::log* new_log,
    const bool is_world_node,
    const std::array<float, 16>& coordinate_space,
    const std::array<float, 16>& new_object_space_parent_transform,
    const std::array<float, 16>& new_object_space_transform,
    const std::array<float, 3> new_world_translate,
    const float new_world_scale, const float new_world_yaw)
{
    l = new_log;
    if (ns = new(std::nothrow) node_state; ns == nullptr)
    {
        l->error("Error allocating node state");
        return result::allocation_failure;
    }
    ns->init(
        l,
        is_world_node,
        coordinate_space,
        new_object_space_parent_transform,
        new_object_space_transform,
        new_world_translate,
        new_world_scale,
        new_world_yaw);
    return result::ok;
}

void node::deinit()
{
    for (node* n : children)
    {
        n->deinit();
        delete n;
    }
    children.erase(children.begin(), children.end());
    delete ns;
    ns = nullptr;
}

void node::set_world_space_translate(const std::array<float, 3>& new_world_space_translate) const
{
    ns->world_space_translate = array_to_vec3(new_world_space_translate);
}

void node::set_world_space_scale(const float new_world_space_scale) const
{
    ns->world_space_scale = new_world_space_scale;
}

void node::set_world_space_yaw(const float new_world_space_yaw) const
{
    ns->world_space_yaw = new_world_space_yaw;
}

void node::set_object_space_bounds(node_bounds new_object_space_bounds) const
{
    ns->object_space_bounds = new_object_space_bounds;
}

std::array<float, 16> node::get_object_space_transform() const
{
    return mat4_to_array(ns->get_object_space_transform());
}

std::array<float, 16> node::get_world_space_transform() const
{
    return mat4_to_array(ns->get_world_space_transform());
}

std::array<float, 16> node::get_to_object_space_transform() const
{
    const glm::mat4 world_space_transform = ns->get_world_space_transform();
    const glm::mat4 world_to_object_space_transform = inverse(world_space_transform);
    const glm::mat4 world_space_normal_transform = transpose(world_to_object_space_transform);

    return mat4_to_array(world_space_normal_transform);
}

node_bounds node::get_world_space_bounds() const
{
    const glm::mat4 world_space_transform = ns->get_world_space_transform();
    const glm::vec4 object_space_min_bounds = {ns->object_space_bounds.min[0], ns->object_space_bounds.min[1], ns->object_space_bounds.min[2], 1.f};
    const glm::vec4 object_space_max_bounds = {ns->object_space_bounds.max[0], ns->object_space_bounds.max[1], ns->object_space_bounds.max[2], 1.f};
    glm::vec4 world_space_min_bounds = world_space_transform * object_space_min_bounds;
    glm::vec4 world_space_max_bounds = world_space_transform * object_space_max_bounds;

    for (glm::length_t i{0}; i < 3; i++)
    {
        if (world_space_min_bounds[i] > world_space_max_bounds[i])
        {
            const float swap = world_space_min_bounds[i];
            world_space_min_bounds[i] = world_space_max_bounds[i];
            world_space_max_bounds[i] = swap;
        }
    }
    return { .min = {world_space_min_bounds[0], world_space_min_bounds[1], world_space_min_bounds[2]}, .max ={world_space_max_bounds[0], world_space_max_bounds[1], world_space_max_bounds[2]}};
}

std::array<float, 3> node::get_world_space_position() const
{
    const auto rv = vec4_to_array(ns->get_world_space_transform() * glm::vec4(0.f, 0.f, 0.f, 1.f));
    return {rv[0], rv[1], rv[2]};
}

void node::update_object_space_parent_transform(const std::array<float, 16>& new_parent_transform) const
{
    ns->object_space_parent_transform = array_to_mat4(new_parent_transform);
    for (const node* n : children)
    {
        n->update_object_space_parent_transform(mat4_to_array(ns->object_space_parent_transform * ns->object_space_transform));
    }
}

void node::populate_graph(std::vector<graphics_object>& graph) const
{
    const glm::mat4 world_space_transform = ns->get_world_space_transform();
    const glm::mat4 world_to_object_space_transform = inverse(world_space_transform);
    const glm::mat3 world_space_normal_transform = glm::transpose(glm::inverse(glm::mat3(world_space_transform)));

    const std::array<float, 16> go_transform = mat4_to_array(world_space_transform);
    const std::array<float, 16> go_to_object_space_transform = mat4_to_array(world_to_object_space_transform);
    const std::array<float, 9> go_normal_transform = mat3_to_array(world_space_normal_transform);

    for (graphics_object go : graphics_objects)
    {
        assert(graph.size() > go.index);
        go.transform = go_transform;
        go.normal_transform = go_normal_transform;
        go.to_object_space_transform = go_to_object_space_transform;
        graph[go.index] = go;
    }
    for (const node* n : children)
    {
        n->populate_graph(graph);
    }
}

void node::debug()
{
    size_t num_surfaces{0};
    for (const auto& go : graphics_objects) num_surfaces += go.surface_data.size();
    l->debug(std::format("game node name: {} num graphic objects: {} num surfaces: {}", name, graphics_objects.size(), num_surfaces));
    for (node* n : children)
    {
        n->debug();
    }
}
