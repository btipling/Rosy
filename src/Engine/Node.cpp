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
        std::array<float, 16> a{};
        const auto pos_r = glm::value_ptr(m);
        for (uint64_t i{0}; i < 16; i++) a[i] = pos_r[i];
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
}

struct node_state
{
    glm::mat4 parent_transform{};
    glm::mat4 transform{};
    glm::mat4 object_space_transform{};
    glm::mat4 normal_transform{};
    glm::vec4 position{};

    void set_transform(const std::array<float, 16>& new_transform, const std::array<float, 16>& new_parent_transform)
    {
        parent_transform = array_to_mat4(new_parent_transform);
        transform = array_to_mat4(new_transform);
        position = parent_transform * transform * glm::vec4(0.f, 0.f, 0.f, 1.f);
        object_space_transform = glm::inverse(static_cast<glm::mat3>(parent_transform * transform));
        normal_transform = glm::transpose(object_space_transform);
    }

    void update_transform(const std::array<float, 16>& new_transform)
    {
        transform = array_to_mat4(new_transform);
        position = parent_transform * transform * glm::vec4(0.f, 0.f, 0.f, 1.f);
        object_space_transform = glm::inverse(static_cast<glm::mat3>(parent_transform * transform));
        normal_transform = glm::transpose(object_space_transform);
    }

    void update_parent_transform(const std::array<float, 16>& new_parent_transform)
    {
        parent_transform = array_to_mat4(new_parent_transform);
        position = transform * glm::vec4(0.f, 0.f, 0.f, 1.f);
    }

    void set_position(const std::array<float, 3>& new_position)
    {
        position = glm::vec4(new_position[0], new_position[1], new_position[2], 1.f);
        transform = glm::mat4(
            transform[0],
            transform[1],
            transform[2],
            position
        );
        object_space_transform = glm::inverse(static_cast<glm::mat3>(parent_transform * transform));
        normal_transform = glm::transpose(object_space_transform);
    }
};

result node::init(rosy::log* new_log, const std::array<float, 16>& new_transform,
                  const std::array<float, 16>& new_parent_transform)
{
    l = new_log;
    ns = new(std::nothrow) node_state;
    if (ns == nullptr)
    {
        l->error("Error allocating node state");
        return result::allocation_failure;
    }
    ns->set_transform(new_transform, new_parent_transform);
    transform = mat4_to_array(ns->parent_transform * ns->transform);
    object_space_transform = mat4_to_array(ns->object_space_transform);
    normal_transform = mat4_to_array(ns->normal_transform);
    position = vec4_to_array(ns->position);
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

result node::set_position(const std::array<float, 3>& new_position)
{
    ns->set_position(new_position);
    transform = mat4_to_array(ns->parent_transform * ns->transform);
    position = vec4_to_array(ns->position);
    object_space_transform = mat4_to_array(ns->object_space_transform);
    normal_transform = mat4_to_array(ns->normal_transform);
    for (node* n : children)
    {
        n->update_parent_transform(transform);
    }
    return result::ok;
}

result node::update_transform(const std::array<float, 16>& new_parent_transform)
{
    ns->update_transform(new_parent_transform);
    transform = mat4_to_array(ns->parent_transform * ns->transform);
    position = vec4_to_array(ns->position);
    if (std::isnan(position[0]) || std::isnan(position[1]) || std::isnan(position[2]))
    {
        l->error("update_transform: error nan");
        return result::error;
    }
    object_space_transform = mat4_to_array(ns->object_space_transform);
    normal_transform = mat4_to_array(ns->normal_transform);
    for (node* n : children)
    {
        n->update_parent_transform(transform);
    }
    return result::ok;
}

void node::update_parent_transform(const std::array<float, 16>& new_parent_transform)
{
    ns->update_parent_transform(new_parent_transform);
    transform = mat4_to_array(ns->parent_transform * ns->transform);
    position = vec4_to_array(ns->position);
    object_space_transform = mat4_to_array(ns->object_space_transform);
    normal_transform = mat4_to_array(ns->normal_transform);
    for (node* n : children)
    {
        n->update_parent_transform(transform);
    }
}

void node::populate_graph(std::vector<graphics_object>& graph) const
{
    for (graphics_object go : graphics_objects)
    {
        assert(graph.size() > go.index);
        go.transform = transform;
        go.normal_transform = normal_transform;
        go.object_space_transform = object_space_transform;
        graph[go.index] = go;
    }
    for (node* n : children)
    {
        n->populate_graph(graph);
    }
}

void node::debug()
{
    size_t num_surfaces{0};
    for (const auto& go : graphics_objects) num_surfaces += go.surface_data.size();
    l->debug(std::format("game node name: {} num graphic objects: {} num surfaces: {}", name, graphics_objects.size(),
                         num_surfaces));
    for (node* n : children)
    {
        n->debug();
    }
}
