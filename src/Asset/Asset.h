#pragma once
#include "Engine/Types.h"
#include "Logger/Logger.h"
#include <array>
#include <string>
#include <vector>

namespace rosy_asset
{
    constexpr uint32_t rosy_format{0x52535946}; // "RSYF"
    constexpr uint32_t current_version{1};

    struct file_header
    {
        uint32_t magic{0};
        uint32_t version{0};
        uint32_t endianness{0};
        std::array<float, 16> coordinate_system{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        uint32_t root_scene{0};
    };

    struct material
    {
        uint8_t double_sided{0};
        std::array<float, 4> base_color_factor{0.f};
        // In Rosy, metallic_factor and roughness_factor are only considered in a metallic workflow when metallic or
        // mixmaps are present. Rosy is a game engine designed to work with assets directly designed for Rosy and
        // is not a generic GLTF or FBX viewer and does not adhere to any external specification.
        float metallic_factor{1.f};
        float roughness_factor{1.f};
        uint8_t alpha_mode{ 0 };
        float alpha_cutoff{ 0.f };
        uint32_t color_image_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t color_sampler_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t normal_image_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t normal_sampler_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t metallic_image_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t metallic_sampler_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t mixmap_image_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t mixmap_sampler_index{UINT32_MAX}; // UINT32_MAX == not present
    };

    struct sampler
    {
        uint16_t min_filter{UINT16_MAX}; // UINT16_MAX == not present
        uint16_t mag_filter{UINT16_MAX}; // UINT16_MAX == not present
        uint16_t wrap_s{0};
        uint16_t wrap_t{0};
    };

    struct surface
    {
        uint32_t start_index{0};
        uint32_t count{0};
        uint32_t material{UINT32_MAX};
        std::array<float, 3> min_bounds{0.f, 0.f, 0.f};
        std::array<float, 3> max_bounds{0.f, 0.f, 0.f};
    };

    struct node
    {
        std::array<float, 3> world_translate{0.f, 0.f, 0.f};
        float world_scale{1.f};
        float world_yaw{0.f};
        // Nodes need a coordinate system because they can be combined from different asset systems at runtime.
        // Not written to file! It is intentionally by default a zero matrix to identify it easily as not valid.
        std::array<float, 16> coordinate_system{};
        //  is_world_node another node property that not written to the file format and helps distinguish nodes
        // that are independent world nodes from nodes that are ancestors of world nodes.
        bool is_world_node{false};
        std::array<float, 16> transform{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        std::uint32_t mesh_id{UINT32_MAX}; // UINT32_MAX == not present
        std::vector<uint32_t> child_nodes;
        std::vector<char> name;
    };

    struct scene
    {
        std::vector<uint32_t> nodes;
    };

    struct position
    {
        std::array<float, 3> vertex{0.f, 0.f, 0.f};
        std::array<float, 3> normal{0.f, 0.f, 0.f};
        std::array<float, 4> tangents{0.f, 0.f, 0.f, 0.f};
        std::array<float, 4> color{1.f, 0.f, 0.f, 1.f};
        std::array<float, 2> texture_coordinates{0.f, 0.f};
    };

    struct mesh
    {
        std::vector<position> positions;
        std::vector<uint32_t> indices;
        std::vector<surface> surfaces;
    };

    // image_type is effectively an enum
    constexpr uint32_t image_type_color{0};
    constexpr uint32_t image_type_normal_map{1};
    constexpr uint32_t image_type_metallic_roughness{2};
    constexpr uint32_t image_type_mixmap{3};

    // More image types as needed will be added.
    struct image
    {
        uint32_t image_type{0};
        std::vector<char> name;
    };

    struct shader
    {
        std::string path{};
        std::vector<char> source;
    };

    struct asset
    {
        std::string asset_path{};
        std::vector<material> materials;
        std::vector<sampler> samplers;
        std::vector<scene> scenes;
        std::vector<node> nodes;
        std::vector<image> images;
        std::vector<mesh> meshes;
        std::vector<shader> shaders;
        std::array<float, 16> asset_coordinate_system{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        uint32_t root_scene{0};

        rosy::result write(const std::shared_ptr<rosy_logger::log> l);
        rosy::result read(std::shared_ptr<rosy_logger::log> l);
        rosy::result read_shaders(const std::shared_ptr<rosy_logger::log>& l);
    };
}
