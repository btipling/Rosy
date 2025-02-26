#pragma once
#include "Types.h"
#include "Telemetry.h"

struct node_state;

namespace rosy
{
    struct node_bounds
    {
        std::array<float, 3> min{
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()
        };
        std::array<float, 3> max{
            std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()
        };
    };

    struct node
    {
        log* l{nullptr};
        node_state* ns{nullptr};
        std::vector<graphics_object> graphics_objects;
        std::string name{};
        std::vector<node*> children;
        node_bounds bounds{};

        [[nodiscard]] result init(
            log* new_log,
            bool is_world_node,
            const std::array<float, 16>& coordinate_space,
            const std::array<float, 16>& new_object_space_parent_transform,
            const std::array<float, 16>& new_object_space_transform,
            const std::array<float, 3> new_world_translate,
            float new_world_scale,
            float new_world_yaw);
        void deinit();
        void set_world_space_translate(const std::array<float, 3>& new_world_space_translate) const;
        void set_world_space_scale(const float new_world_space_scale) const;
        void set_world_space_yaw(float new_world_space_yaw) const;

        // get_object_space_transform does not return any world space transforms and does not escape the node's asset inherited coordinate system!
        // The only purpose this function can serve is within **the same** mesh's object space derived from the same asset! Cannot be used with other nodes!
        // Like if you wanted to move a hat around already on the model.
        [[nodiscard]] std::array<float, 16> get_object_space_transform() const;

        // get_world_space_transform returns the asset's representation in the world after all object space transforms have been applied, from asset to rosy's
        // world space coordinate system and then any world space transforms
        [[nodiscard]] std::array<float, 16> get_world_space_transform() const;

       
        [[nodiscard]] std::array<float, 3> get_world_space_position() const;

        void update_object_space_parent_transform(const std::array<float, 16>& new_parent_transform) const;
        void populate_graph(std::vector<graphics_object>& graph) const;
        void debug();
    };
}
