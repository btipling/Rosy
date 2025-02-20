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
        std::array<float, 16> parent_transform{};
        std::array<float, 16> transform{};
        std::array<float, 16> normal_transform{};
        std::array<float, 16> object_space_transform{};
        std::array<float, 4> position{};
        std::array<float, 3> custom_translate{};
        float custom_scale{1.f};
        float custom_yaw{0.f};
        node_bounds bounds{};

        [[nodiscard]] result init(log* new_log, const std::array<float, 16>& new_transform, const std::array<float, 16>& new_parent_transform,
                                  const std::array<float, 3> new_custom_translate, float new_custom_scale, float new_custom_yaw);
        void deinit();
        [[nodiscard]] result set_position(const std::array<float, 3>& new_position);
        [[nodiscard]] result update_transform(const std::array<float, 16>& new_parent_transform);
        void update_parent_transform(const std::array<float, 16>& new_parent_transform);
        void populate_graph(std::vector<graphics_object>& graph) const;
        void debug();
    };
}
