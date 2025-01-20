#pragma once
#include <array>
#include <string>
#include <vector>
#include "../Engine/Types.h"

namespace rosy_packager {

	constexpr uint32_t rosy_format{ 0x52535946 };  // "RSYF"
	constexpr uint32_t current_version{ 1 };

	struct file_header {
		uint32_t magic;
		uint32_t version;
		uint32_t endianness;
		uint32_t root_scene{ 0 };
	};

	struct material
	{
		uint8_t double_sided{ 0 };
		std::array<float, 4> base_color_factor{ 0.f };
		float metallic_factor{ 0.f };
		float roughness_factor{ 0.f };
	};

	struct surface
	{
		uint32_t start_index{ 0 };
		uint32_t count{ 0 };
		size_t material{ 0 };
	};

	struct node
	{
		std::vector<uint32_t> child_nodes;
		std::array<float, 16> transform;
		std::uint32_t mesh_id; // if larger than meshes list, it means the node doesn't have a mesh
	};

	struct scene
	{
		std::vector<uint32_t> nodes;
	};

	struct position
	{
		std::array<float, 3> vertex{ 0.f, 0.f, 0.f };
		std::array<float, 3> normal{ 0.f, 0.f, 0.f };
		std::array<float, 4> color{ 1.f, 0.f, 0.f, 1.f };
	};

	struct mesh
	{
		std::vector<position> positions;
		std::vector<uint32_t> indices;
		std::vector<surface> surfaces;
	};

	struct shader
	{
		std::string path{};
		std::vector<char> source;
	};

	struct asset
	{
		std::string asset_path{};
		std::vector<mesh> meshes;
		std::vector<material> materials;
		std::vector<scene> scenes;
		std::vector<node> nodes;
		std::vector<shader> shaders;
		uint32_t root_scene{ 0 };

		rosy::result write();
		rosy::result read();
		rosy::result read_shaders();
	};
}