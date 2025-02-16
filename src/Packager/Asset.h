#pragma once
#include "../Engine/Types.h"
#include "../Engine/Telemetry.h"
#include <array>
#include <string>
#include <vector>

namespace rosy_packager
{
    constexpr uint32_t rosy_format{0x52535946}; // "RSYF"
    constexpr uint32_t current_version{1};

    struct file_header
    {
        uint32_t magic{0};
        uint32_t version{0};
        uint32_t endianness{0};
        uint32_t root_scene{0};
    };

    struct material
    {
        uint8_t double_sided{0};
        std::array<float, 4> base_color_factor{0.f};
        float metallic_factor{0.f};
        float roughness_factor{0.f};
        uint32_t color_image_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t color_sampler_index{UINT32_MAX}; // UINT32_MAX == not present
        uint8_t alpha_mode{0};
        float alpha_cutoff{0.f};
        uint32_t normal_image_index{UINT32_MAX}; // UINT32_MAX == not present
        uint32_t normal_sampler_index{UINT32_MAX}; // UINT32_MAX == not present
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
        uint32_t material{0};
        std::array<float, 3> min_bounds{0.f, 0.f, 0.f};
        std::array<float, 3> max_bounds{0.f, 0.f, 0.f};
    };

    struct node
    {
        std::array<float, 16> transform;
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
        std::vector<uint32_t> child_meshes;
    };

    struct image
    {
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
        uint32_t root_scene{0};

        rosy::result write(const rosy::log* l);
        rosy::result read(rosy::log* l);
        rosy::result read_shaders(const rosy::log* l);
    };
}
