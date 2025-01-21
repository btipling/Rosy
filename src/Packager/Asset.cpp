#include "Asset.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace rosy_packager;

// Rosy File Format:
// 1. Header
// 2. GLTF Size Layouts: std::array<size_t,2> 
		// index 0 - num materials
		// index 1 - num samplers
		// index 2 - num scenes
		// index 3 - num nodes
		// index 4 - num images
		// index 5 - num meshes
// 3. a std::vector<material> of material size given
// 4. a std::vector<sampler> of sampler size given
// 5. Per scene
// 5a. Scene size layout std::array<size_t, 1>
//       // index 0 - num nodes -> a std::vector<uint32_t> of node indices
// 6. Per Node
// 6a. Node size layout: std::array<size_t, 3>
//       // index 0 - num transforms -> always 1 to represent a single std::array<float, 16>
//       // index 1 -> num mesh ids -> always 1 to represent a single uint32_t
//       // index 2 -> num child_nodes -> a std::vector<uint32_t> to represent child node indices
// 7. Per Image
// 7a.Image size layout: std::array<size_t, 1>
//       // index 0 - num characters -> a std::vector<char> to represent an image name
// 8. Per mesh:
// 8.a Mesh size layout: std::array<size_t,4>
		// index 0 - num positions -> a std::vector<position> of positions size given
		// index 1 - num indices -> a std::vector<uint32_t> of indices size given
		// index 2 - num surfaces -> a std::vector<surface> of surfaces size given
		// index 3 - num child meshes -> a std::vector<uint32_t> of child mesh size given

rosy::result asset::write()
{

	// OPEN FILE FOR WRITING BINARY

	FILE* stream{ nullptr };

	std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';

	if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "wb"); err != 0)
	{
		std::cerr << std::format("failed to open {}, {}", asset_path, err) << '\n';
		return rosy::result::open_failed;
	}

	// WRITE RSY FORMAT HEADER

	{
		constexpr size_t num_headers = 1;
		const file_header header{
			.magic = rosy_format,
			.version = current_version,
			.endianness = std::endian::native == std::endian::little ? 1 : 0,
			.root_scene = root_scene,
		};
		size_t res = fwrite(&header, sizeof(header), num_headers, stream);
		if (res != num_headers) {
			std::cerr << std::format("failed to write {}/{} headers", res, num_headers) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} headers", res) << '\n';
	}

	// WRITE GLTF SIZES FOR ASSET RESOURCES

	{
		const size_t num_materials{ materials.size() };
		const size_t num_samplers{ samplers.size() };
		const size_t num_scenes{ scenes.size() };
		const size_t num_nodes{ nodes.size() };
		const size_t num_images{ images.size() };
		const size_t num_meshes{ meshes.size() };

		constexpr size_t lookup_sizes = 1;
		const std::array<size_t, 6> sizes{
			num_materials,
			num_samplers,
			num_scenes,
			num_nodes,
			num_images,
			num_meshes,
		};
		size_t res = fwrite(&sizes, sizeof(sizes), lookup_sizes, stream);
		if (res != lookup_sizes) {
			std::cerr << std::format("failed to write {}/{} num_gltf_sizes", res, lookup_sizes) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} sizes, num_materials: {}, num_samplers: {}, num_scenes: {}, num_nodes: {}, num_images: {}, num_meshes: {}",
			res, 
			num_materials,
			num_samplers,
			num_scenes,
			num_nodes,
			num_images,
			num_meshes) << '\n';
	}

	// WRITE MATERIALS

	{
		size_t res = fwrite(materials.data(), sizeof(material), materials.size(), stream);
		if (res != materials.size()) {
			std::cerr << std::format("failed to write {}/{} materials", res, materials.size()) << '\n';
			return rosy::result::write_failed;
		}
		std::cout << std::format("wrote {} materials", res) << '\n';
	}

	// WRITE ALL THE SCENES ONE AT A TIME

	for (const auto& [scene_nodes] : scenes) {

		// WRITE ONE SCENE SIZE

		{
			constexpr size_t lookup_sizes = 1;
			const size_t num_scene_nodes{ scene_nodes.size() };
			const std::array<size_t, 1> scene_sizes{ num_scene_nodes };
			size_t res = fwrite(&scene_sizes, sizeof(scene_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to write {}/{} scene_sizes", res, lookup_sizes) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format(
				"wrote {} sizes, num_nodes: {}",
				res, num_scene_nodes) << '\n';
		}

		// WRITE ONE SCENE NODES

		{
			size_t res = fwrite(scene_nodes.data(), sizeof(uint32_t), scene_nodes.size(), stream);
			if (res != scene_nodes.size()) {
				std::cerr << std::format("failed to write {}/{} scene nodes", res, scene_nodes.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} scene nodes", res) << '\n';
		}
	}

	// WRITE ALL NODES ONE AT A TIME

	for (const auto& [transform, mesh_id, child_nodes] : nodes) {

		// WRITE ONE NODE SIZE

		constexpr size_t num_transforms{ 1 };
		constexpr size_t num_mesh_ids{ 1 };
		{
			const size_t num_child_nodes{ child_nodes.size() };
			constexpr size_t lookup_sizes = 1;
			const std::array<size_t, 3> node_sizes{ num_transforms, num_mesh_ids, num_child_nodes };
			size_t res = fwrite(&node_sizes, sizeof(node_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to write {}/{} node_sizes", res, lookup_sizes) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format(
				"wrote {} sizes, num_transforms: {} num_mesh_ids: {} num_child_nodes: {}",
				res, num_transforms, num_mesh_ids, num_child_nodes) << '\n';
		}

		// WRITE ONE NODE TRANSFORM

		{
			size_t res = fwrite(&transform, sizeof(transform), num_transforms, stream);
			if (res != num_transforms) {
				std::cerr << std::format("failed to write {}/{} node transforms", res, num_transforms) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} node transforms", res) << '\n';
		}

		// WRITE ONE NODE MESH INDEX

		{
			size_t res = fwrite(&mesh_id, sizeof(mesh_id), num_mesh_ids, stream);
			if (res != num_mesh_ids) {
				std::cerr << std::format("failed to write {}/{} node mesh ids", res, num_mesh_ids) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} node mesh ids", res) << '\n';
		}

		// WRITE ONE NODE CHILD NODES

		{
			size_t res = fwrite(child_nodes.data(), sizeof(uint32_t), child_nodes.size(), stream);
			if (res != child_nodes.size()) {
				std::cerr << std::format("failed to write {}/{} node child nodes", res, child_nodes.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} node child nodes", res) << '\n';
		}
	}

	// WRITE ALL IMAGES ONE AT A TIME

	for (const auto& [names] : images) {

		// WRITE ONE IMAGE SIZE

		{
			const size_t num_chars{ names.size() };
			constexpr size_t lookup_sizes = 1;
			const std::array<size_t, 1> image_sizes{ num_chars };
			size_t res = fwrite(&image_sizes, sizeof(image_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to write {}/{} image_sizes", res, lookup_sizes) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format(
				"wrote {} sizes, num image name chars: {}",
				res, num_chars) << '\n';
		}

		// WRITE ONE IMAGE NAME

		{
			size_t res = fwrite(names.data(), sizeof(char), names.size(), stream);
			if (res != names.size()) {
				std::cerr << std::format("failed to write {}/{} image name chars", res, names.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} image name chars", res) << '\n';
		}

	}

	// WRITE ALL MESHES ONE AT A TIME

	for (const auto& [positions, indices, surfaces, child_meshes] : meshes) {

		// WRITE ONE MESH SIZE

		{
			const size_t num_positions{ positions.size() };
			const size_t num_indices{ indices.size() };
			const size_t num_surfaces{ surfaces.size() };
			const size_t num_child_meshes{ child_meshes.size() };
			constexpr size_t lookup_sizes = 1;
			const std::array<size_t, 4> mesh_sizes{ num_positions, num_indices, num_surfaces, num_child_meshes };
			size_t res = fwrite(&mesh_sizes, sizeof(mesh_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to write {}/{} num_mesh_sizes", res, lookup_sizes) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format(
				"wrote {} sizes, num_positions: {} num_indices: {} num_surfaces: {}, child_meshes: {}",
				res, num_positions, num_indices, num_surfaces, num_child_meshes) << '\n';
		}

		// WRITE ONE MESH POSITIONS

		{
			size_t res = fwrite(positions.data(), sizeof(position), positions.size(), stream);
			if (res != positions.size()) {
				std::cerr << std::format("failed to write {}/{} positions", res, positions.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} positions", res) << '\n';
		}

		// WRITE ONE MESH INDICES

		{
			size_t res = fwrite(indices.data(), sizeof(uint32_t), indices.size(), stream);
			if (res != indices.size()) {
				std::cerr << std::format("failed to write {}/{} indices", res, indices.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} indices", res) << '\n';
		}

		// WRITE ONE MESH SURFACES

		{
			size_t res = fwrite(surfaces.data(), sizeof(surface), surfaces.size(), stream);
			if (res != surfaces.size()) {
				std::cerr << std::format("failed to write {}/{} surfaces", res, surfaces.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} surfaces", res) << '\n';
		}

		// WRITE ONE MESH CHILD MESHES

		{
			size_t res = fwrite(child_meshes.data(), sizeof(uint32_t), child_meshes.size(), stream);
			if (res != child_meshes.size()) {
				std::cerr << std::format("failed to write {}/{} child meshes", res, child_meshes.size()) << '\n';
				return rosy::result::write_failed;
			}
			std::cout << std::format("wrote {} child meshes", res) << '\n';
		}
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';

	return rosy::result::ok;
}

rosy::result asset::read()
{

	// OPEN FILE FOR READING BINARY

	FILE* stream{ nullptr };

	{
		std::cout << std::format("current file path: {}", std::filesystem::current_path().string()) << '\n';
		if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "rb"); err != 0)
		{
			std::cerr << std::format("failed to open {}, {}", asset_path, err) << '\n';
			return rosy::result::open_failed;
		}
	}

	// READ RSY FORMAT HEADER
	{
		constexpr size_t num_headers = 1;
		file_header header{
			.magic = 0,
			.version = 0,
			.endianness = 0,
			.root_scene = 0,
		};
		size_t res = fread(&header, sizeof(header), num_headers, stream);
		if (res != num_headers) {
			std::cerr << std::format("failed to read {}/{} headers", res, num_headers) << '\n';
			return rosy::result::read_failed;
		}
		if (header.magic != rosy_format)
		{
			std::cerr << std::format("failed to read, magic mismatch, got: {} should be {}", header.magic, rosy_format) << '\n';
			return rosy::result::read_failed;
		}
		if (header.version != current_version)
		{
			std::cerr << std::format("failed to read, version mismatch file is version {} current version is {}", header.version, current_version) << '\n';
			return rosy::result::read_failed;
		}
		constexpr uint32_t is_little_endian = std::endian::native == std::endian::little ? 1 : 0;
		if (header.endianness != is_little_endian)
		{
			std::cerr << std::format("failed to read, endianness mismatch file is {} system is {}", header.endianness, is_little_endian) << '\n';
			return rosy::result::read_failed;
		}
		root_scene = header.root_scene;
		std::cout << std::format("read {} headers", res) << '\n';
		std::cout << std::format("format version: {} is little endian: {} root scene: {}",
			header.version, is_little_endian, root_scene) << '\n';
	}

	// WRITE GLTF SIZES FOR ALL ASSET RESOURCES

	size_t num_materials{ 0 };
	size_t num_samplers{ 0 };
	size_t num_scenes{ 0 };
	size_t num_nodes{ 0 };
	size_t num_images{ 0 };
	size_t num_meshes{ 0 };
	{
		constexpr size_t lookup_sizes = 1;
		std::array<size_t, 6> num_gltf_sizes{ 0, 0, 0, 0, 0, 0 };
		size_t res = fread(&num_gltf_sizes, sizeof(num_gltf_sizes), lookup_sizes, stream);
		if (res != lookup_sizes) {
			std::cerr << std::format("failed to read {}/{} num_gltf_sizes", res, lookup_sizes) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} sizes", res) << '\n';

		num_materials = num_gltf_sizes[0];
		num_samplers = num_gltf_sizes[1];
		num_scenes = num_gltf_sizes[2];
		num_nodes = num_gltf_sizes[3];
		num_images = num_gltf_sizes[4];
		num_meshes = num_gltf_sizes[5];
	}

	materials.resize(num_materials);
	samplers.resize(num_samplers);
	scenes.reserve(num_scenes);
	nodes.reserve(num_nodes);
	meshes.reserve(num_meshes);

	// READ GLTF MATERIALS

	{
		size_t res = fread(materials.data(), sizeof(material), num_materials, stream);
		if (res != num_materials) {
			std::cerr << std::format("failed to read {}/{} materials", res, num_materials) << '\n';
			return rosy::result::read_failed;
		}
		std::cout << std::format("read {} positions", res) << '\n';
	}

	// READ ALL THE SCENES ONE AT A TIME

	for (size_t i{ 0 }; i < num_scenes; i++) {
		scene s{};

		// READ ONE SCENE SIZE

		size_t num_scene_nodes{ 0 };
		{
			constexpr size_t lookup_sizes = 1;
			std::array<size_t, 1> scene_sizes{ 0 };
			size_t res = fread(&scene_sizes, sizeof(scene_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to read {}/{} scene_sizes", res, lookup_sizes) << '\n';
				return rosy::result::read_failed;
			}
			num_scene_nodes = scene_sizes[0];
			std::cout << std::format(
				"read {} sizes, num_nodes: {} for scene",
				res, num_scene_nodes) << '\n';
		}

		s.nodes.resize(num_scene_nodes);

		// READ ONE SCENE NODES

		{
			size_t res = fread(s.nodes.data(), sizeof(uint32_t), num_scene_nodes, stream);
			if (res != num_scene_nodes) {
				std::cerr << std::format("failed to read {}/{} scene nodes", res, num_scene_nodes) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} scene nodes", res) << '\n';
		}
		scenes.push_back(s);
	}

	// READ ALL NODES ONE AT A TIME

	for (size_t i{ 0 }; i < num_nodes; i++) {
		node n{};

		// READ ONE NODE SIZE

		size_t num_transforms{ 0 };
		size_t num_mesh_ids{ 0 };
		size_t num_child_nodes{ 0 };
		{
			constexpr size_t lookup_sizes = 1;
			std::array<size_t, 3> node_sizes{ 0, 0, 0 };
			size_t res = fread(&node_sizes, sizeof(node_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to read {}/{} node_sizes", res, lookup_sizes) << '\n';
				return rosy::result::read_failed;
			}
			num_transforms = node_sizes[0];
			num_mesh_ids = node_sizes[1];
			num_child_nodes = node_sizes[2];
			std::cout << std::format(
				"read {} sizes, num_transforms: {} num_mesh_ids: {} num_child_nodes: {}",
				res, num_transforms, num_mesh_ids, num_child_nodes) << '\n';
			assert(num_transforms == 1);
			assert(num_mesh_ids == 1);
		}

		n.child_nodes.resize(num_child_nodes);

		// READ ONE NODE TRANSFORM

		{
			size_t res = fread(&n.transform, sizeof(std::array<float, 16>), num_transforms, stream);
			if (res != num_transforms) {
				std::cerr << std::format("failed to read {}/{} node transform", res, num_transforms) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} node transform", res) << '\n';
		}

		// READ ONE NODE MESH ID

		{
			size_t res = fread(&n.mesh_id, sizeof(uint32_t), num_mesh_ids, stream);
			if (res != num_mesh_ids) {
				std::cerr << std::format("failed to read {}/{} node mesh ids", res, num_mesh_ids) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} node mesh ids", res) << '\n';
		}

		// READ ONE NODE CHILD NODES

		{
			size_t res = fread(n.child_nodes.data(), sizeof(uint32_t), num_child_nodes, stream);
			if (res != num_child_nodes) {
				std::cerr << std::format("failed to read {}/{} node child nodes", res, num_child_nodes) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} node child nodes", res) << '\n';
		}

		// ADD MESH TO ASSET

		nodes.push_back(n);
	}

	// READ ALL IMAGES ONE AT A TIME

	for (size_t i{ 0 }; i < num_images; i++) {
		image img{};

		// READ ONE IMAGE SIZE

		size_t num_chars{ 0 };
		{
			constexpr size_t lookup_sizes = 1;
			std::array<size_t, 1> image_sizes{ 0 };
			size_t res = fread(&image_sizes, sizeof(image_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to read {}/{} image_sizes", res, lookup_sizes) << '\n';
				return rosy::result::read_failed;
			}
			num_chars = image_sizes[0];
			std::cout << std::format(
				"read {} sizes, num_chars: {}",
				res, num_chars) << '\n';
		}

		img.name.resize(num_chars);

		// READ ONE IMAGE NAME

		{
			size_t res = fread(img.name.data(), sizeof(char), num_chars, stream);
			if (res != num_chars) {
				std::cerr << std::format("failed to read {}/{} image name chars", res, num_chars) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} image name chars", res) << '\n';
		}

		// ADD IMAGE TO ASSET

		images.push_back(img);
	}

	// READ ALL MESHES ONE AT A TIME

	for (size_t i{ 0 }; i < num_meshes; i++) {
		mesh m{};

		// READ ONE MESH SIZE

		size_t num_positions{ 0 };
		size_t num_indices{ 0 };
		size_t num_surfaces{ 0 };
		size_t num_child_meshes{ 0 };
		{
			constexpr size_t lookup_sizes = 1;
			std::array<size_t, 4> mesh_sizes{ 0, 0, 0, 0 };
			size_t res = fread(&mesh_sizes, sizeof(mesh_sizes), lookup_sizes, stream);
			if (res != lookup_sizes) {
				std::cerr << std::format("failed to read {}/{} num_mesh_sizes", res, lookup_sizes) << '\n';
				return rosy::result::read_failed;
			}
			num_positions = mesh_sizes[0];
			num_indices = mesh_sizes[1];
			num_surfaces = mesh_sizes[2];
			num_child_meshes = mesh_sizes[3];
			std::cout << std::format(
				"read {} sizes, num_positions: {} num_indices: {} num_surfaces: {} num_child_meshes: {}",
				res, num_positions, num_indices, num_surfaces, num_child_meshes) << '\n';
		}

		m.positions.resize(num_positions);
		m.indices.resize(num_indices);
		m.surfaces.resize(num_surfaces);
		m.child_meshes.resize(num_child_meshes);

		// READ ONE MESH POSITIONS

		{
			size_t res = fread(m.positions.data(), sizeof(position), num_positions, stream);
			if (res != num_positions) {
				std::cerr << std::format("failed to read {}/{} positions", res, num_positions) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} positions", res) << '\n';
		}

		// READ ONE MESH INDICES

		{
			size_t res = fread(m.indices.data(), sizeof(uint32_t), num_indices, stream);
			if (res != num_indices) {
				std::cerr << std::format("failed to read {}/{} indices", res, num_indices) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} indices", res) << '\n';
		}

		// READ ONE MESH SURFACES

		{
			size_t res = fread(m.surfaces.data(), sizeof(surface), num_surfaces, stream);
			if (res != num_surfaces) {
				std::cerr << std::format("failed to read {}/{} surfaces", res, num_surfaces) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} surfaces", res) << '\n';
		}

		// READ ONE MESH CHILD MESHES

		{
			size_t res = fread(m.child_meshes.data(), sizeof(uint32_t), num_child_meshes, stream);
			if (res != num_child_meshes) {
				std::cerr << std::format("failed to read {}/{} child meshes", res, num_child_meshes) << '\n';
				return rosy::result::read_failed;
			}
			std::cout << std::format("read {} child meshes", res) << '\n';
		}

		// ADD MESH TO ASSET

		meshes.push_back(m);
	}

	int num_closed = fclose(stream);

	std::cout << std::format("closed {} files", num_closed) << '\n';
	return rosy::result::ok;
}

rosy::result asset::read_shaders()
{
	// ReSharper disable once CppUseStructuredBinding
	for (shader& s : shaders)
	{
		std::ifstream file(s.path, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			std::cerr << std::format("failed to open shader {}", s.path) << '\n';
			return rosy::result::open_failed;
		}

		const std::streamsize file_size = file.tellg();
		if (file_size < 1)
		{
			std::cerr << std::format("invalid shader source {}", s.path) << '\n';
			return rosy::result::read_failed;
		}
		std::vector<char> buffer(file_size);
		s.source.resize(file_size);
		file.seekg(0);
		file.read(s.source.data(), file_size);
		file.close();
		if (static_cast<std::streamsize>(s.source.size()) != file_size)
		{
			std::cerr << std::format("failed to read shader source {}", s.path) << '\n';
			return rosy::result::read_failed;
		}

		std::cout << std::format("shader file size: {}", file_size) << '\n';
	}
	std::cout << std::format("read {} shaders", shaders.size()) << '\n';
	return rosy::result::ok;
}

